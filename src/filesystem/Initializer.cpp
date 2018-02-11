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

#include "filesystem/Initializer.h"

#include "boost/thread/once.hpp"

namespace QS {

namespace FileSystem {

boost::once_flag Initializer::m_initOnceFlag = BOOST_ONCE_INIT;
Initializer::InitFunctionQueue *Initializer::m_queue = NULL;

Initializer::Initializer(const PriorityInitFuncPair &initFuncPair) {
  SetInitializer(initFuncPair);
}

void Initializer::RunInitializers() {
  if (m_queue != NULL) {
    while (!m_queue->empty()) {
      (m_queue->top().second)();
      m_queue->pop();
    }
  }
}

void Initializer::RemoveInitializers() {
  if (m_queue != NULL) {
    while (!m_queue->empty()) {
      m_queue->pop();
    }
  }
}

void Initializer::SetInitializer(const PriorityInitFuncPair &pair) {
  boost::call_once(m_initOnceFlag, Init);
  m_queue->push(pair);
}

void Initializer::Init() {
  if (m_queue == NULL) {
    m_queue = new InitFunctionQueue;
  }
}

}  // namespace FileSystem
}  // namespace QS
