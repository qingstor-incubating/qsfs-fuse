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

#ifndef QSFS_CLIENT_QSTRANSFERMANAGER_H_
#define QSFS_CLIENT_QSTRANSFERMANAGER_H_

#include <string>
#include <utility>

#include "boost/shared_ptr.hpp"

#include "client/QSError.h"
#include "client/TransferManager.h"

namespace QS {

namespace Data {
class Cache;
class IOStream;
}  // namespace Data

namespace Client {

class Part;
class TransferHandle;

class QSTransferManager : public TransferManager {
 public:
  explicit QSTransferManager(const TransferManagerConfigure &config)
      : TransferManager(config) {}

  ~QSTransferManager() {}

 public:
  // Download a file
  //
  // @param  : file path, file offset, size, bufStream
  // @return : transfer handle
  boost::shared_ptr<TransferHandle> DownloadFile(
      const std::string &filePath, off_t offset, uint64_t size,
      boost::shared_ptr<std::iostream> bufStream, bool async = false);

  // Retry a failed download
  //
  // @param  : transfer handle to retry
  // @return : transfer handle after been retried
  boost::shared_ptr<TransferHandle> RetryDownload(
      const boost::shared_ptr<TransferHandle> &handle,
      boost::shared_ptr<std::iostream> bufStream, bool async = false);

  // Upload a file
  //
  // @param  : file path, file size
  // @return : transfer handle
  boost::shared_ptr<TransferHandle> UploadFile(
      const std::string &filePath, uint64_t fileSize, time_t fileMtimeSince,
      const boost::shared_ptr<QS::Data::Cache> &cache, bool async = false);

  // Retry a failed upload
  //
  // @param  : tranfser handle to retry
  // @return : transfer handle after been retried
  boost::shared_ptr<TransferHandle> RetryUpload(
      const boost::shared_ptr<TransferHandle> &handle, time_t fileMtimeSince,
      const boost::shared_ptr<QS::Data::Cache> &cache, bool async = false);

  // Abort a multipart upload
  //
  // @param  : tranfer handle to abort
  // @return : void
  //
  // By default, multipart upload will remain in a Failed state if they fail,
  // or a Cancelled state if they were cancelled. Leaving failed state around
  // still costs the owner of the bucket money. If you know you will not going
  // to retry it, abort the multipart upload request after cancelled or failed.
  void AbortMultipartUpload(const boost::shared_ptr<TransferHandle> &handle);

 private:
  bool PrepareDownload(const boost::shared_ptr<TransferHandle> &handle);
  void DoSinglePartDownload(const boost::shared_ptr<TransferHandle> &handle,
                            bool async = false);
  void DoMultiPartDownload(const boost::shared_ptr<TransferHandle> &handle,
                           bool async = false);
  void DoDownload(const boost::shared_ptr<TransferHandle> &handle,
                  bool async = false);

  bool PrepareUpload(const boost::shared_ptr<TransferHandle> &handle);
  void DoSinglePartUpload(const boost::shared_ptr<TransferHandle> &handle,
                          const boost::shared_ptr<QS::Data::Cache> &cache,
                          time_t mtimeSince, bool async = false);
  void DoMultiPartUpload(const boost::shared_ptr<TransferHandle> &handle,
                         const boost::shared_ptr<QS::Data::Cache> &cache,
                         time_t mtimeSince, bool async = false);
  void DoUpload(const boost::shared_ptr<TransferHandle> &handlebool,
                const boost::shared_ptr<QS::Data::Cache> &cache,
                time_t mtimeSince, bool async = false);

 private:
  // Internal use only
  std::pair<ClientError<QSError::Value>, std::string> SingleDownloadWrapper(
      const boost::shared_ptr<TransferHandle> &handle,
      const boost::shared_ptr<Part> &part);

  std::pair<ClientError<QSError::Value>, std::string> MultipleDownloadWrapper(
      const boost::shared_ptr<TransferHandle> &handle,
      const boost::shared_ptr<Part> &part);

  ClientError<QSError::Value> SingleUploadWrapper(
      const boost::shared_ptr<TransferHandle> &handle,
      const boost::shared_ptr<QS::Data::IOStream> &stream);

  ClientError<QSError::Value> MultipleUploadWrapper(
      const boost::shared_ptr<TransferHandle> &handle,
      const boost::shared_ptr<Part> &part,
      const boost::shared_ptr<QS::Data::IOStream> &stream);
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_QSTRANSFERMANAGER_H_
