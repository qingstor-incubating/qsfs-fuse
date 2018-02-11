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

#ifndef QSFS_CLIENT_TRANSFERMANAGERFACTORY_H_
#define QSFS_CLIENT_TRANSFERMANAGERFACTORY_H_

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"

namespace QS {

namespace Client {

class TransferManager;
class TransferManagerConfigure;

class TransferManagerFactory : private boost::noncopyable {
 public:
  ~TransferManagerFactory() {}

 public:
  static boost::shared_ptr<TransferManager> Create(
      const TransferManagerConfigure &config);

 private:
  TransferManagerFactory() {}
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_TRANSFERMANAGERFACTORY_H_
