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

#include <data/StreamUtils.h>

#include <assert.h>

#include <iostream>

#include "boost/shared_ptr.hpp"

#include "base/LogMacros.h"

namespace QS {

namespace Data {

namespace StreamUtils {

using boost::shared_ptr;
using std::iostream;

size_t GetStreamOutputSize(const shared_ptr<iostream> &stream) {
  size_t sz = 0;
  assert(stream);
  if (stream) {
    std::iostream::pos_type curPos = stream->tellp();
    if (curPos == std::iostream::pos_type(-1)) {
      DebugError("Fail to get stream current pos");
      return 0;
    }
    stream->seekp(0, std::ios_base::end);
    sz = static_cast<size_t>(stream->tellp() - curPos);
    stream->seekp(curPos);
  } else {
    DebugWarning("Try to lookup the size of a null output stream");
  }
  return sz;
}

size_t GetStreamInputSize(const shared_ptr<iostream> &stream) {
  size_t sz = 0;
  assert(stream);
  if (stream) {
    std::iostream::pos_type curPos = stream->tellg();
    if (curPos == std::iostream::pos_type(-1)) {
      DebugError("Fail to get stream current pos");
      return 0;
    }
    stream->seekg(0, std::ios_base::end);
    sz = static_cast<size_t>(stream->tellg() - curPos);
    stream->seekg(curPos);
  } else {
    DebugWarning("Try to lookup the size of a null input stream");
  }
  return sz;
}

size_t GetStreamSize(const shared_ptr<iostream> &stream) {
  size_t sz = 0;
  assert(stream);
  if (stream) {
    std::iostream::pos_type curPos = stream->tellg();
    if (curPos == std::iostream::pos_type(-1)) {
      DebugError("Fail to get stream current pos");
      return 0;
    }
    stream->seekg(0, std::ios_base::end);
    sz = static_cast<size_t>(stream->tellg());
    stream->seekg(curPos);
  } else {
    DebugWarning("Try to lookup the size of a null input stream");
  }
  return sz;
}

}  // namespace StreamUtils
}  // namespace Data
}  // namespace QS
