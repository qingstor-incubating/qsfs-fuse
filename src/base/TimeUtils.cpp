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

#include <base/TimeUtils.h>

#include <string.h>  // for memset
#include <time.h>    // for strftime

#include <string>

namespace QS {

namespace TimeUtils {

using std::string;

// --------------------------------------------------------------------------
time_t RFC822GMTToSeconds(const string &date) {
  if (date.empty()) {
    return 0L;
  }
  struct tm res;
  memset(&res, 0, sizeof(struct tm));

  // date example: Tue, 15 Nov 1994 08:12:31 GMT
  static const char *formatGMT = "%a, %d %b %Y %H:%M:%S GMT";
  strptime(date.c_str(), formatGMT, &res);
  return timegm(&res);  // GMT
}

// --------------------------------------------------------------------------
string SecondsToRFC822GMT(time_t time) {
  char date[100];
  memset(date, 0, sizeof(date));

  static const char *formatGMT = "%a, %d %b %Y %H:%M:%S GMT";
  strftime(date, sizeof(date), formatGMT, gmtime(&time));
  return date;
}

// --------------------------------------------------------------------------
bool IsExpire(time_t t, int32_t expireDurationInMin) {
  if (expireDurationInMin < 0) {
    return false;
  } else {
    time_t now = time(NULL);
    return t + static_cast<time_t>(expireDurationInMin * 60) < now;
  }
}

}  // namespace TimeUtils
}  // namespace QS
