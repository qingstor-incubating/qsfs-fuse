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

#include "client/RetryStrategy.h"

#include <stdint.h>

#include "configure/Default.h"
#include "configure/Options.h"

namespace QS {

namespace Client {

bool RetryStrategy::ShouldRetry(const ClientError<QSError::Value> &error,
                                uint16_t attemptedRetryTimes) const {
  return attemptedRetryTimes >= m_maxRetryTimes ? false : error.ShouldRetry();
}

uint32_t RetryStrategy::CalculateDelayBeforeNextRetry(
    uint16_t attemptedRetryTimes) const {
  return attemptedRetryTimes == 0 ? 0
                                  : (1 << attemptedRetryTimes) * m_scaleFactor;
}

RetryStrategy GetDefaultRetryStrategy() {
  return RetryStrategy(QS::Configure::Default::GetDefaultTransactionRetries(),
                       Retry::DefaultScaleFactor);
}

RetryStrategy GetCustomRetryStrategy() {
  const QS::Configure::Options &options = QS::Configure::Options::Instance();
  return RetryStrategy(options.GetRetries(), Retry::DefaultScaleFactor);
}

}  // namespace Client
}  // namespace QS
