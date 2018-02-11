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

#ifndef QSFS_CLIENT_NULLTRANSFERMANAGER_H_
#define QSFS_CLIENT_NULLTRANSFERMANAGER_H_

#include <string>

#include "boost/shared_ptr.hpp"

#include "client/TransferManager.h"
#include "data/Cache.h"

namespace QS {

namespace Client {

class TransferHandle;

class NullTransferManager : public TransferManager {
 public:
  explicit NullTransferManager(const TransferManagerConfigure &config)
      : TransferManager(config) {}

  ~NullTransferManager() {}

 public:
  boost::shared_ptr<TransferHandle> DownloadFile(
      const std::string &filePath, off_t offset, uint64_t size,
      boost::shared_ptr<std::iostream> downStream, bool async = false) {
    return boost::shared_ptr<TransferHandle>();
  }

  boost::shared_ptr<TransferHandle> RetryDownload(
      const boost::shared_ptr<TransferHandle> &handle,
      boost::shared_ptr<std::iostream> bufStream, bool async = false) {
    return boost::shared_ptr<TransferHandle>();
  }

  boost::shared_ptr<TransferHandle> UploadFile(
      const std::string &filePath, uint64_t fileSize, time_t fileMTimeSince,
      const boost::shared_ptr<QS::Data::Cache> &cache, bool async = false) {
    return boost::shared_ptr<TransferHandle>();
  }

  boost::shared_ptr<TransferHandle> RetryUpload(
      const boost::shared_ptr<TransferHandle> &handle, time_t fileMTimeSince,
      const boost::shared_ptr<QS::Data::Cache> &cache, bool async = false) {
    return boost::shared_ptr<TransferHandle>();
  }

  void AbortMultipartUpload(const boost::shared_ptr<TransferHandle> &handle) {}
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_NULLTRANSFERMANAGER_H_
