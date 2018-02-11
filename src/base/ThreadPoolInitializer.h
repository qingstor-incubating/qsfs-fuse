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

#ifndef QSFS_BASE_THREADPOOLINITIALIZER_H_
#define QSFS_BASE_THREADPOOLINITIALIZER_H_

#include <set>

#include "base/Singleton.hpp"
#include "configure/IncludeFuse.h"

namespace QS {

namespace FileSystem {
void* qsfs_init(struct fuse_conn_info* conn);
}  // namespace FileSystem

namespace Threading {

class ThreadPool;

class ThreadPoolInitializer : public Singleton<ThreadPoolInitializer> {
 public:
  ~ThreadPoolInitializer() {}

 public:
  void Register(ThreadPool* threadPool);
  void UnRegister(ThreadPool* threadPool);

 private:
  void DoInitialize();
  friend void* QS::FileSystem::qsfs_init(struct fuse_conn_info* conn);

 private:
  ThreadPoolInitializer() {}

  std::set<ThreadPool*> m_threadPools;

  friend class Singleton<ThreadPoolInitializer>;
};

}  // namespace Threading
}  // namespace QS

#endif  // QSFS_BASE_THREADPOOLINITIALIZER_H_
