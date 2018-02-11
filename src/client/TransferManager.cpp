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

#include "client/TransferManager.h"

#include <assert.h>

#include <cmath>
#include <utility>
#include <vector>

#include "boost/foreach.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/once.hpp"

#include "base/LogMacros.h"
#include "base/ThreadPool.h"
#include "base/ThreadPoolInitializer.h"
#include "client/NullClient.h"
#include "data/ResourceManager.h"

namespace QS {

namespace Client {

using boost::make_shared;
using boost::shared_ptr;
using QS::Data::Resource;
using QS::Data::ResourceManager;
using QS::Threading::ThreadPool;
using std::vector;

boost::once_flag initOnceFlag = BOOST_ONCE_INIT;

// --------------------------------------------------------------------------
TransferManager::TransferManager(const TransferManagerConfigure &config)
    : m_configure(config), m_client(make_shared<NullClient>()) {
  if (GetBufferCount() > 0) {
    m_bufferManager = shared_ptr<ResourceManager>(new ResourceManager);
  }
  if (GetMaxParallelTransfers() > 0) {
    m_executor =
        shared_ptr<ThreadPool>(
            new QS::Threading::ThreadPool(config.m_maxParallelTransfers));
    QS::Threading::ThreadPoolInitializer::Instance().Register(m_executor.get());
  }
}

// --------------------------------------------------------------------------
TransferManager::~TransferManager() {
  if (!m_bufferManager) {
    return;
  }
  vector<Resource> resources =
      m_bufferManager->ShutdownAndWait(GetBufferCount());
  BOOST_FOREACH(Resource &resource, resources) {
    if (resource) {
      resource.reset();
    }
  }
}

// --------------------------------------------------------------------------
size_t TransferManager::GetBufferCount() const {
  return GetBufferSize() == 0
             ? 0
             : static_cast<size_t>(
                   std::ceil(static_cast<long double>(GetBufferMaxHeapSize()) /
                             static_cast<long double>(GetBufferSize())));
}

// --------------------------------------------------------------------------
void TransferManager::SetClient(const shared_ptr<Client> &client) {
  assert(client);
  if (client) {
    m_client = client;
    boost::call_once(initOnceFlag,
                     boost::bind(boost::type<void>(),
                                 &TransferManager::InitializeResources, this));
  } else {
    DebugError("Null client parameter");
  }
}

// --------------------------------------------------------------------------
void TransferManager::InitializeResources() {
  if (!m_bufferManager) {
    DebugError("Buffer Manager is null");
    return;
  }
  for (uint64_t i = 0; i < GetBufferMaxHeapSize(); i += GetBufferSize()) {
    m_bufferManager->PutResource(Resource(new vector<char>(GetBufferSize())));
  }
}

}  // namespace Client
}  // namespace QS
