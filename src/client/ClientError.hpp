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

#ifndef QSFS_CLIENT_CLIENTERROR_HPP_
#define QSFS_CLIENT_CLIENTERROR_HPP_

#include <string>

namespace QS {

namespace Client {

template <typename ERROR_TYPE>
class ClientError {
 public:
  ClientError() : m_isRetryable(false) {}
  ClientError(ERROR_TYPE error, const std::string &exceptionName,
        const std::string &errorMsg, bool isRetryable)
      : m_error(error),
        m_exceptionName(exceptionName),
        m_message(errorMsg),
        m_isRetryable(isRetryable) {}
  ClientError(ERROR_TYPE error, bool isRetryable)
      : m_error(error),
        m_exceptionName(),
        m_message(),
        m_isRetryable(isRetryable) {}

 public:
  const ERROR_TYPE GetError() const { return m_error; }
  const std::string &GetExceptionName() const { return m_exceptionName; }
  const std::string &GetMessage() const { return m_message; }
  bool ShouldRetry() const { return m_isRetryable; }

  void SetExceptionName(const std::string &exceptionName) {
    m_exceptionName = exceptionName;
  }
  void SetMessage(const std::string &message) { m_message = message; }

 private:
  ERROR_TYPE m_error;
  std::string m_exceptionName;
  std::string m_message;
  bool m_isRetryable;
};

}  // namespace Client
}  // namespace QS


#endif  // QSFS_CLIENT_CLIENTERROR_HPP_
