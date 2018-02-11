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

#include "data/ResourceManager.h"

#include <assert.h>

#include <functional>
#include <vector>

#include "boost/bind.hpp"
#include "boost/thread/locks.hpp"

#include "base/LogMacros.h"

namespace QS {

namespace Data {

using boost::bind;
using boost::lock_guard;
using boost::mutex;
using boost::unique_lock;
using std::vector;

// --------------------------------------------------------------------------
bool ResourceManager::ResourcesAvailable() {
  lock_guard<mutex> lock(m_queueLock);
  return !m_resources.empty() && !IsShutdown();
}

// --------------------------------------------------------------------------
void ResourceManager::PutResource(const Resource &resource) {
  if (resource) {
    m_resources.push_back(resource);
  }
}

// --------------------------------------------------------------------------
Resource ResourceManager::Acquire() {
  unique_lock<mutex> lock(m_queueLock);
  m_semaphore.wait(
      lock, bind(boost::type<bool>(), &ResourceManager::Predicate, this));

  // Should not go here
  assert(!IsShutdown());
  DebugErrorIf(IsShutdown(),
               "Trying to acquire resouce BUT resouce manager is shutdown");

  Resource resource = m_resources.back();
  m_resources.pop_back();

  return resource;
}

// --------------------------------------------------------------------------
void ResourceManager::Release(const Resource &resource) {
  unique_lock<mutex> lock(m_queueLock);
  if (resource) {
    m_resources.push_back(resource);
  }
  lock.unlock();
  m_semaphore.notify_one();
}

// --------------------------------------------------------------------------
vector<Resource> ResourceManager::ShutdownAndWait(size_t resourceCount) {
  unique_lock<mutex> lock(m_queueLock);
  SetShutdown(true);
  m_semaphore.wait(lock, bind(boost::type<bool>(), std::greater_equal<size_t>(),
                              m_resources.size(), resourceCount));
  vector<Resource> resources = m_resources;
  m_resources.clear();
  return resources;
}

// --------------------------------------------------------------------------
bool ResourceManager::IsShutdown() const {
  lock_guard<mutex> lock(m_shutdownLock);
  return m_shutdown;
}

// --------------------------------------------------------------------------
void ResourceManager::SetShutdown(bool shutdown) {
  lock_guard<mutex> lock(m_shutdownLock);
  m_shutdown = shutdown;
}

// --------------------------------------------------------------------------
bool ResourceManager::Predicate() const {
  return IsShutdown() || !m_resources.empty();
}

}  // namespace Data
}  // namespace QS
