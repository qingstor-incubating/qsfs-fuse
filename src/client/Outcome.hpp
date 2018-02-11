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

#ifndef QSFS_CLIENT_OUTCOME_HPP_
#define QSFS_CLIENT_OUTCOME_HPP_

#include <utility>

namespace QS {

namespace Client {

// Representing the outcome of making a request.
// It will contain either a successful result or the failure error.
// The caller must check whether the outcome was a success before
// attempting to access the result or the error.
template <typename Result, typename Error>
class Outcome {
 public:
  Outcome() : m_success(false) {}

  explicit Outcome(const Result &result) : m_result(result), m_success(true) {}
  explicit Outcome(const Error &error) : m_error(error), m_success(false) {}

  Outcome(const Outcome &rhs)
      : m_result(rhs.m_result),
        m_error(rhs.m_error),
        m_success(rhs.m_success) {}

  Outcome &operator=(const Outcome &rhs) {
    if (&rhs != this) {
      m_result = rhs.m_result;
      m_error = rhs.m_error;
      m_success = rhs.m_success;
    }
    return *this;
  }

  ~Outcome() {}

 public:
  const Result &GetResult() const { return m_result; }
  Result &GetResult() { return m_result; }
  const Error &GetError() const { return m_error; }
  bool IsSuccess() const { return m_success; }

 private:
  Result m_result;
  Error m_error;
  bool m_success;
};

}  // namespace Client
}  // namespace QS


#endif  // QSFS_CLIENT_OUTCOME_HPP_
