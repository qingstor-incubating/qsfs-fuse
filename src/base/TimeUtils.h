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

#ifndef QSFS_BASE_TIMEUTILS_H_
#define QSFS_BASE_TIMEUTILS_H_

#include <stdint.h>
#include <time.h>

#include <string>

namespace QS {

namespace TimeUtils {

// Convert rfc822 GMT date to time in seconds
//
// @param  : date string in rfc822 GMT format
// @return : time in seconds
time_t RFC822GMTToSeconds(const std::string &date);

// Convert time to rfc822 GMT date string
//
// @param  : time in seconds
// @return : date string in rfc822 GMT format
std::string SecondsToRFC822GMT(time_t time);

// Check if given time expired
//
// @param  : time to check, expire duration in min
// @return : bool
bool IsExpire(time_t t, int32_t expireDurationInMin);

}  // namespace TimeUtils
}  // namespace QS


#endif  // QSFS_BASE_TIMEUTILS_H_
