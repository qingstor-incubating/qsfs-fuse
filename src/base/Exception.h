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

#ifndef QSFS_BASE_EXCEPTION_H_
#define QSFS_BASE_EXCEPTION_H_

#include <stdexcept>
#include <string>

namespace QS {

namespace Exception {

struct QSException : public std::runtime_error {
  explicit QSException(const std::string& msg) : std::runtime_error(msg) {}
  explicit QSException(const char* msg)
      : std::runtime_error(std::string(msg)) {}

  std::string get() const { return this->what(); }
};

}  // namespace Exception
}  // namespace QS


#endif  // QSFS_BASE_EXCEPTION_H_
