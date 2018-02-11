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

#include "base/TaskHandle.h"

#include "boost/bind.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/shared_mutex.hpp"

#include "base/ThreadPool.h"

namespace QS {

namespace Threading {

using boost::lock_guard;
using boost::mutex;
using boost::shared_lock;
using boost::shared_mutex;
using boost::unique_lock;

// --------------------------------------------------------------------------
TaskHandle::TaskHandle(ThreadPool& threadPool)
    : m_continue(true),
      m_threadPool(threadPool),
      m_thread(bind(boost::type<void>(), &TaskHandle::operator(), this)) {}

// --------------------------------------------------------------------------
TaskHandle::~TaskHandle() {
  Stop();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

// --------------------------------------------------------------------------
void TaskHandle::Stop() {
  lock_guard<shared_mutex> lock(m_continueLock);
  m_continue = false;
}

// --------------------------------------------------------------------------
bool TaskHandle::ShouldContinue() const {
  shared_lock<shared_mutex> lock(m_continueLock);
  return m_continue;
}

// --------------------------------------------------------------------------
void TaskHandle::operator()() {
  while (ShouldContinue()) {
    while (ShouldContinue() && m_threadPool.HasTasks()) {
      Task* task = m_threadPool.PopTask();
      if (task) {
        (*task)();
        delete task;
        task = NULL;
      }
    }

    unique_lock<mutex> lock(m_threadPool.m_syncLock);
    m_threadPool.m_syncConditionVar.wait(
        lock, bind(boost::type<bool>(), &TaskHandle::Predicate, this));
  }
}

// --------------------------------------------------------------------------
bool TaskHandle::Predicate() const {
  return !ShouldContinue() || m_threadPool.HasTasks();
}

}  // namespace Threading
}  // namespace QS
