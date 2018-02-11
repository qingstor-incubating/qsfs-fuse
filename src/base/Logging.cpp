// +-------------------------------------------------------------------------
// | Copyright (C) 2017 Yunify, Inc.
// +-------------------------------------------------------------------------
// | Licensed under the Apache License, Version 2.0 (the "License");
// | You may not use this work except in compliance with the License.
// | You may obtain a copy of the License in the LICENSE file, or at:
// |
// | http://www.apache.org/licenses/LICENSE-2.0
// |
// | Unless required by applicable law or agreed to in writing, software
// | distributed under the License is distributed on an "AS IS" BASIS,
// | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// | See the License for the specific language governing permissions and
// | limitations under the License.
// +-------------------------------------------------------------------------

#include "base/Logging.h"

#include <errno.h>
#include <string.h>  // for strerr

#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "boost/bind.hpp"
#include "boost/thread/once.hpp"
#include "glog/logging.h"

#include "base/Exception.h"
#include "base/LogLevel.h"
#include "base/Utils.h"
#include "configure/Default.h"

namespace {

void InitializeGLog() {
  google::InitGoogleLogging(QS::Configure::Default::GetProgramName());
}

}  // namespace

namespace QS {

namespace Logging {

using QS::Exception::QSException;
using std::pair;
using std::string;

static boost::once_flag initOnce = BOOST_ONCE_INIT;

// --------------------------------------------------------------------------
void Log::Initialize(const string &logdir) {
  boost::call_once(initOnce, boost::bind(boost::type<void>(),
                                         &Log::DoInitialize, this, logdir));
}

// --------------------------------------------------------------------------
void Log::SetLogLevel(LogLevel::Value level) {
  m_logLevel = level;
  FLAGS_minloglevel = static_cast<int>(level);
}

// --------------------------------------------------------------------------
void Log::DoInitialize(const string &logdir) {
  if (logdir.empty()) {
    FLAGS_logtostderr = 1;
  } else {
    m_logDirectory = logdir;
    // Notes for glog, most settings start working immediately after you upate
    // FLAGS_*. The exceptions are the flags related to desination files.
    // So need to set FLAGS_log_dir before calling google::InitGoogleLogging.
    FLAGS_log_dir = logdir.c_str();

    if (!QS::Utils::CreateDirectoryIfNotExists(logdir)) {
      throw QSException("Unable to create log directory " + logdir + " : " +
                        strerror(errno));
    }

    if (!QS::Utils::HavePermission(logdir).first) {
      throw QSException("Could not create logging file at " + logdir +
                        ": Permission denied");
    }
  }

  InitializeGLog();
}

// --------------------------------------------------------------------------
void Log::ClearLogDirectory() const {
  if (m_logDirectory.empty()) {
    std::cerr << "Log message to console , nothing to clear" << std::endl;
  } else {
    pair<bool, string> outcome =
        QS::Utils::DeleteFilesInDirectory(m_logDirectory, false);
    if (!outcome.first) {
      std::cerr << "Unable to clear log directory : ";
      std::cerr << outcome.second << ". But Continue..." << std::endl;
    }
  }
}

}  // namespace Logging
}  // namespace QS
