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

#ifndef QSFS_CLIENT_UTILS_H_
#define QSFS_CLIENT_UTILS_H_

#include <stddef.h>  // for size_t

#include <sys/types.h>  // for off_t

#include <string>
#include <utility>

#include "boost/tuple/tuple.hpp"

namespace QS {

namespace Client {

namespace Utils {

// TODO(jim): when sdk support meta data
// refer s3fs::get_mode to add utils

// Build request header of 'Range'
//
// @param  : start, size
// @return : string with format of "bytes=start_offset-stop_offset"
std::string BuildRequestRange(off_t start, size_t size);

// Build request header of 'Range'
//
// @param  : start
// @return : string with format of "bytes=start_offset-"
std::string BuildRequestRangeStart(off_t start);

// Build request header of 'Range'
//
// @param  : suffix length
// @return : string with format of "bytes=-suffix_length"
std::string BuildRequestRangeEnd(off_t suffixLen);

// Parse response header of 'Range'
//
// @param  : 'Content-Range' or 'Content-Copy-Range'
//          which has format of "bytes start_offset-stop_offset/file_size"
// @return : start, body length(stop - start + 1), total file_size
boost::tuple<off_t, size_t, size_t> ParseResponseContentRange(
    const std::string &responseRange);

// Parse request header of 'Range'
//
// @param  : request range with format of "bytes=start_offset-stop_offset"
// @return : start, size(stop - start + 1)
std::pair<off_t, size_t> ParseRequestContentRange(
    const std::string &requestRange);

}  // namespace Utils
}  // namespace Client
}  // namespace QS



#endif  // QSFS_CLIENT_UTILS_H_
