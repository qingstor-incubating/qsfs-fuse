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

#include "data/IOStream.h"

#include <vector>

#include "data/StreamBuf.h"

namespace QS {

namespace Data {
using std::vector;

IOStream::IOStream(size_t bufSize)
    : Base(new StreamBuf(Buffer(new vector<char>(bufSize)), bufSize)) {}
IOStream::IOStream(Buffer buf, size_t lengthToRead)
    : Base(new StreamBuf(buf, lengthToRead)) {}

IOStream::~IOStream() {
  if (rdbuf()) {
    delete (rdbuf());
  }
}

}  // namespace Data
}  // namespace QS
