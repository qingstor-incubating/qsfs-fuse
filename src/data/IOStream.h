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

#ifndef QSFS_DATA_IOSTREAM_H_
#define QSFS_DATA_IOSTREAM_H_

#include <stddef.h>

#include <iostream>
#include <string>  // for std::char_traits

#include "boost/noncopyable.hpp"

#include "data/StreamBuf.h"

namespace QS {

namespace Data {

/**
 * An iostream to use QS::StreamBuf under the hood.
 */
class IOStream : public std::basic_iostream<char, std::char_traits<char> >,
                 private boost::noncopyable {
  typedef std::basic_iostream<char, std::char_traits<char> > Base;

 public:
  explicit IOStream(size_t bufSize);
  IOStream(Buffer buf, size_t lengthToRead);

  ~IOStream();
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_IOSTREAM_H_
