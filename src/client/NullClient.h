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

#ifndef QSFS_CLIENT_NULLCLIENT_H_
#define QSFS_CLIENT_NULLCLIENT_H_

#include <string>
#include <vector>

#include "client/Client.h"

namespace QS {
namespace Data {
class Cache;
class DirectoryTree;
}  // namespace Data

namespace Client {

class NullClient : public Client {
 public:
  NullClient()
      : Client(boost::shared_ptr<ClientImpl>(),
               boost::shared_ptr<QS::Threading::ThreadPool>()) {}

  ~NullClient() {}

 public:
  ClientError<QSError::Value> HeadBucket();

  ClientError<QSError::Value> DeleteFile(
      const std::string &filePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);
  ClientError<QSError::Value> MakeFile(const std::string &filePath);
  ClientError<QSError::Value> MakeDirectory(const std::string &dirPath);
  ClientError<QSError::Value> MoveFile(
      const std::string &filePath, const std::string &newFilePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);
  ClientError<QSError::Value> MoveDirectory(const std::string &sourceDirPath,
                                            const std::string &targetDirPath,
                                            bool async = false);

  ClientError<QSError::Value> DownloadFile(
      const std::string &filePath,
      boost::shared_ptr<std::iostream> buffer, const std::string &range,
      std::string *eTag);

  ClientError<QSError::Value> InitiateMultipartUpload(
      const std::string &filePath, std::string *uploadId);

  ClientError<QSError::Value> UploadMultipart(
      const std::string &filePath, const std::string &uploadId, int partNumber,
      uint64_t contentLength, boost::shared_ptr<std::iostream> buffer);

  ClientError<QSError::Value> SymLink(const std::string &filePath,
                                      const std::string &linkPath);

  ClientError<QSError::Value> CompleteMultipartUpload(
      const std::string &filePath, const std::string &uploadId,
      const std::vector<int> &sortedPartIds);

  ClientError<QSError::Value> AbortMultipartUpload(const std::string &filePath,
                                                   const std::string &uploadId);

  ClientError<QSError::Value> UploadFile(
      const std::string &filePath, uint64_t fileSize,
      boost::shared_ptr<std::iostream> buffer);

  ClientError<QSError::Value> ListDirectory(
      const std::string &dirPath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree);

  ClientError<QSError::Value> Stat(
      const std::string &path,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      time_t modifiedSince = 0, bool *modified = NULL);
  ClientError<QSError::Value> Statvfs(struct statvfs *stvfs);
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_NULLCLIENT_H_
