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

#include "base/LogLevel.h"

#include <string>

#include "base/StringUtils.h"

namespace QS {

namespace Logging {

using std::string;

string GetLogLevelName(LogLevel::Value logLevel) {
  string name;
  switch (logLevel) {
    case LogLevel::Info:
      name = "INFO";
      break;
    case LogLevel::Warn:
      name = "WARN";
      break;
    case LogLevel::Error:
      name = "ERROR";
      break;
    case LogLevel::Fatal:
      name = "FATAL";
      break;
    default:
      break;
  }
  return name;
}

LogLevel::Value GetLogLevelByName(const std::string &name) {
  LogLevel::Value level = LogLevel::Info;
  if (name.empty()) {
    return level;
  }

  string name_lowercase = QS::StringUtils::ToLower(name);
  if (name_lowercase == "warn" || name_lowercase == "warning") {
    level = LogLevel::Warn;
  } else if (name_lowercase == "error") {
    level = LogLevel::Error;
  } else if (name_lowercase == "fatal") {
    level = LogLevel::Fatal;
  }

  return level;
}

string GetLogLevelPrefix(LogLevel::Value logLevel) {
  return "[" + GetLogLevelName(logLevel) + "] ";
}

}  // namespace Logging
}  // namespace QS
