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

#include "client/Client.h"

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"

#include "base/ThreadPool.h"
#include "base/ThreadPoolInitializer.h"
#include "client/ClientImpl.h"

namespace QS {

namespace Client {

using QS::Threading::ThreadPool;
using boost::make_shared;
using boost::scoped_ptr;
using boost::shared_ptr;

// --------------------------------------------------------------------------
Client::Client(const shared_ptr<ClientImpl> &impl,
               const shared_ptr<QS::Threading::ThreadPool> &executor,
               RetryStrategy retryStratety)
    : m_impl(impl), m_executor(executor), m_retryStrategy(retryStratety) {
  QS::Threading::ThreadPoolInitializer::Instance().Register(m_executor.get());
}

// --------------------------------------------------------------------------
Client::~Client() {
  // do nothing
}

// --------------------------------------------------------------------------
void Client::RetryRequestSleep(
    boost::posix_time::milliseconds sleepTime) const {
  boost::unique_lock<boost::mutex> lock(m_retryLock);
  m_retrySignal.timed_wait(lock, sleepTime);
}

}  // namespace Client
}  // namespace QS
