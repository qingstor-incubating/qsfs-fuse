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

#include <iostream>
#include <sstream>
#include <string>

#include "boost/foreach.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"

#include "base/Exception.h"
#include "base/LogMacros.h"
#include "base/Logging.h"
#include "base/UtilsWithLog.h"
#include "client/ClientConfiguration.h"
#include "client/Credentials.h"
#include "configure/Default.h"
#include "configure/Options.h"
#include "filesystem/Initializer.h"
#include "filesystem/MimeTypes.h"

using boost::make_shared;
using boost::shared_ptr;
using QS::Client::ClientConfiguration;
using QS::Client::CredentialsProvider;
using QS::Client::DefaultCredentialsProvider;
using QS::Client::GetCredentialsProviderInstance;
using QS::Client::InitializeClientConfiguration;
using QS::Client::InitializeCredentialsProvider;
using QS::Exception::QSException;
using QS::Configure::Default::GetMimeFiles;
using QS::FileSystem::Initializer;
using QS::FileSystem::Priority;
using QS::FileSystem::PriorityInitFuncPair;
using QS::UtilsWithLog::FileExists;
using std::string;

// --------------------------------------------------------------------------
void LoggingInitializer() {
  const QS::Configure::Options &options = QS::Configure::Options::Instance();
  if (options.IsForeground()) {
    QS::Logging::Log::Instance().Initialize();
  } else {
    QS::Logging::Log::Instance().Initialize(options.GetLogDirectory());
  }
  QS::Logging::Log &log = QS::Logging::Log::Instance();
  if (options.IsDebug()) {
    log.SetDebug(true);
  }
  log.SetLogLevel(options.GetLogLevel());
  if (options.IsClearLogDir()) {
    log.ClearLogDirectory();
  }
}

// --------------------------------------------------------------------------
void CredentialsInitializer() {
  const QS::Configure::Options &options = QS::Configure::Options::Instance();
  if (!FileExists(options.GetCredentialsFile())) {
    throw QSException("qsfs credentials file " + options.GetCredentialsFile() +
                      " does not exist");
  } else {
    InitializeCredentialsProvider(
        make_shared<DefaultCredentialsProvider>(options.GetCredentialsFile()));
  }
}

// --------------------------------------------------------------------------
void ClientConfigurationInitializer() {
  InitializeClientConfiguration(
      make_shared<ClientConfiguration>(GetCredentialsProviderInstance()));
  ClientConfiguration::Instance().InitializeByOptions();
}

// --------------------------------------------------------------------------
void MimeTypesInitializer() {
  string mimeFile = string();
  BOOST_FOREACH(const string &filePath, GetMimeFiles()) {
    if (FileExists(filePath)) {
      mimeFile = filePath;
      break;
    }
  }
  if (mimeFile.empty()) {
    // string files = string();
    // BOOST_FOREACH(const string &filePath, GetMimeFiles()) {
    //   files += filePath;
    //   files += ";";
    // }
    // Info("Unable to find mime types [path=" + files + "]");
    QS::FileSystem::InitializeMimeTypes("");  // default initialize
  } else {
    QS::FileSystem::InitializeMimeTypes(mimeFile);
  }
}

// --------------------------------------------------------------------------
void PrintCommandLineOptions() {
  // Notice: this should only be invoked after logging initialization
  const QS::Configure::Options &options = QS::Configure::Options::Instance();
  std::stringstream ss;
  ss << "<<Command Line Options>> ";
  ss << options << std::endl;
  DebugInfo(ss.str());
}

namespace {

// Register the initializers
static Initializer logInitializer(PriorityInitFuncPair(Priority::First,
                                                       LoggingInitializer));
static Initializer credentialsInitializer(
    PriorityInitFuncPair(Priority::Second, CredentialsInitializer));
static Initializer clientConfigInitializer(
    PriorityInitFuncPair(Priority::Third, ClientConfigurationInitializer));

static Initializer mimeTypesInitializer(
    PriorityInitFuncPair(Priority::Fourth, MimeTypesInitializer));

// Priority must be lower than log initializer
static Initializer printCommandLineOpts(
    PriorityInitFuncPair(Priority::Fifth, PrintCommandLineOptions));

}  // namespace
