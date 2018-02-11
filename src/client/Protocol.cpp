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

#include "client/Protocol.h"

#include <string>

#include "base/LogMacros.h"
#include "base/StringUtils.h"

namespace QS {

namespace Client {

namespace Http {

using std::string;

static const char* const HTTP_NAME = "http";
static const char* const HTTPS_NAME = "https";

string ProtocolToString(Protocol::Value protocol) {
  if (protocol == Protocol::HTTP) {
    return HTTP_NAME;
  } else if (protocol == Protocol::HTTPS) {
    return HTTPS_NAME;
  } else {
    DebugWarning(
        "Trying to get protocol name with unrecognized protocol type, default "
        "protocol of https returned");
  }

  return HTTPS_NAME;
}

Protocol::Value StringToProtocol(const string& name) {
  string str = StringUtils::ToLower(StringUtils::Trim(name, ' '));
  if (str == HTTP_NAME) {
    return Protocol::HTTP;
  } else if (str == HTTPS_NAME) {
    return Protocol::HTTPS;
  } else {
    DebugWarning(
        "Trying to get protocol with unrecognized protocol name, default "
        "protocol of https returned");
  }

  return Protocol::HTTPS;
}

}  // namespace Http
}  // namespace Client
}  // namespace QS
