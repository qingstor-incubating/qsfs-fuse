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

#ifndef QSFS_BASE_LOGGING_H_
#define QSFS_BASE_LOGGING_H_

#include <string>

#include "base/LogLevel.h"
#include "base/Singleton.hpp"

// Declare in global namespace before class Log, since friend declarations
// can only introduce names in the surrounding namespace.
extern void LoggingInitializer();

namespace QS {

namespace Logging {

//
// Log
//
// Must call Initialize to get log ready, and it's one-time initialization.
// Specify a directory to log message to file under it,
// Or log message to console with no specifying.
//
class Log : public Singleton<Log> {
 public:
  LogLevel::Value GetLogLevel() const { return m_logLevel; }
  bool IsDebug() const { return m_isDebug; }

  //
  // Initialize
  // MUST call initialize to get log ready, this is one-time initialization.
  // Pass the log directory to log the message to specified path.
  // Pass null will log the message to console, this is default.
  //
  // @param  : log dir
  // @return : none
  void Initialize(const std::string &logdir = std::string());

 private:
  void SetLogLevel(LogLevel::Value level);
  void SetDebug(bool debug) { m_isDebug = debug; }
  void DoInitialize(const std::string &logdir);
  void ClearLogDirectory() const;

 private:
  Log()
      : m_logLevel(LogLevel::Info),
        m_logDirectory(std::string()),
        m_isDebug(false) {}

  LogLevel::Value m_logLevel;
  std::string m_logDirectory;  // log to console if it's empty
  bool m_isDebug;

  friend void ::LoggingInitializer();
  friend class LoggingTest;
  friend class FatalLoggingDeathTest;
  friend class Singleton<Log>;
};

}  // namespace Logging
}  // namespace QS

#endif  // QSFS_BASE_LOGGING_H_
