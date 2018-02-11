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

#ifndef QSFS_DATA_RESOURCEMANAGER_H_
#define QSFS_DATA_RESOURCEMANAGER_H_

#include <stddef.h>  // for size_t

#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"

namespace QS {

namespace Client {
class TransferManager;
class QSTransferManager;
struct ReceivedHandlerMultipleDownload;
struct ReceivedHandlerMultipleUpload;
}  // namespace  Client

namespace Data {

typedef boost::shared_ptr<std::vector<char> >Resource;

/**
 * Resouce manager with Acquire/Release semantics.
 *
 * Acquire will block waiting on an avaiable resource.
 * Release will cause one blocked acquisition to unblock.
 * You must call ShutdownAndWait when finished with the resouce manager,
 * this will unblock the listening thread and give you a chance to
 * clean up the resouce if needed.
 * After calling ShutdownAndWait, you must not call Acquire any more.
 */
class ResourceManager : private boost::noncopyable {
 public:
  ResourceManager() : m_shutdown(false) {}

  ~ResourceManager() {}

 public:
  // Return whether or not resources are currently available for acquisition.
  //
  // @param  : void
  // @return : bool
  //
  // This is only a hint to denote the resource availability at this instant.
  // Return ture means some resource available though another thread may
  // grab them from under you.
  bool ResourcesAvailable();

  // Return whether resource manager is shutdown
  //
  // @param  : void
  // @return : bool
  bool IsShutdown() const;

 private:
  // Put resouce to the pool
  //
  // @param  : resouce
  // @return : void
  //
  // Does not block or even touch the semaphore.
  void PutResource(const Resource &resource);

  // Returns a resource with exclusive ownership
  //
  // @param  : void
  // @return : released resouce
  //
  // You must call Release on the resource when you are finished
  // or other threads will block waiting to acquire it.
  Resource Acquire();

  // Release a resource back to the pool.
  //
  // @param  : resource
  // @return : void
  //
  // This will unblock one thread waiting Acquire call if any are waiting.
  void Release(const Resource &resource);

  // Waits for all acquired resources to be released, then empty the queue
  //
  // @param  : resource count
  // @return : released resources
  //
  // You must call ShutdownAndWait when finished with the resouce manager,
  // this will unblock the listening thread and give you a chance to
  // clean up the resouce if needed.
  // After calling ShutdownAndWait, you must not call Acquire any more.
  std::vector<Resource> ShutdownAndWait(size_t resourceCount);

  // Set shutdown falg
  //
  // @param  : bool
  // @return : void
  void SetShutdown(bool shutdown);

  // Predeict if wait when try to acqurie
  // For internal use only.
  //
  // @param  : void
  // @return : bool
  bool Predicate() const;

 private:
  std::vector<Resource> m_resources;
  boost::mutex m_queueLock;
  boost::condition_variable m_semaphore;
  bool m_shutdown;
  mutable boost::mutex m_shutdownLock;

  friend class QS::Client::TransferManager;
  friend class QS::Client::QSTransferManager;
  friend struct QS::Client::ReceivedHandlerMultipleDownload;
  friend struct QS::Client::ReceivedHandlerMultipleUpload;
  friend class ResourceManagerTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_RESOURCEMANAGER_H_
