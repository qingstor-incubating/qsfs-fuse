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

#ifndef QSFS_BASE_STRINGUTILS_H_
#define QSFS_BASE_STRINGUTILS_H_

#include <stdio.h>

#include <string>
#include <vector>

#include "boost/type_traits/is_pointer.hpp"

namespace QS {

namespace StringUtils {

template <typename P>
std::string PointerAddress(P p) {
  if (boost::is_pointer<P>::value) {
    int sz = snprintf(NULL, 0, "%p", p);
    std::vector<char> buf(sz + 1);
    snprintf(&buf[0], sz + 1, "%p", p);
    std::string ss(buf.begin(), buf.end());
    return ss;
  }
  return std::string();
}

std::string ToLower(const std::string &str);
std::string ToUpper(const std::string &str);

std::string LTrim(const std::string &str, unsigned char c);
std::string RTrim(const std::string &str, unsigned char c);
std::string Trim(const std::string &str, unsigned char c);

// Convert access mode to string
//
// @param  : access mode
// @return : string of a combination of {R_OK, W_OK, X_OK}, e.g. "R_OK&W_OK"
std::string AccessMaskToString(int amode);

// Convert file mode to string
//
// @param  : file mode
// @return : string in form of [-rwxXst]
std::string ModeToString(mode_t mode);

// Get file type letter
//
// @param  : file mode
// @return : char
char GetFileTypeLetter(mode_t mode);

// Format path
//
// @param  : path
// @return : formatted string
std::string FormatPath(const std::string &path);
std::string FormatPath(const std::string &from, const std::string &to);

}  // namespace StringUtils
}  // namespace QS

#endif  // QSFS_BASE_STRINGUTILS_H_
