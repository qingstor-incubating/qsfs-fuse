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

#include "client/NullClient.h"

#include <memory>
#include <string>
#include <vector>

#include "client/QSError.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"

namespace QS {

namespace Client {

using boost::shared_ptr;
using std::string;

namespace {
ClientError<QSError::Value> GoodState() {
  return ClientError<QSError::Value>(QSError::GOOD, false);
}
}  // namespace

ClientError<QSError::Value> NullClient::HeadBucket() {
  return GoodState();
}

ClientError<QSError::Value> NullClient::DeleteFile(
    const string &filePathconst,
    const shared_ptr<QS::Data::DirectoryTree> &dirTree,
    const shared_ptr<QS::Data::Cache> &cache) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::MakeFile(const string &filePath) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::MakeDirectory(const string &dirPath) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::MoveFile(
    const string &filePath, const string &newFilePath,
    const shared_ptr<QS::Data::DirectoryTree> &dirTree,
    const shared_ptr<QS::Data::Cache> &cache) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::MoveDirectory(
    const string &sourceDirPath, const string &targetDirPath, bool async) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::DownloadFile(
    const string &filePath, shared_ptr<std::iostream> buffer,
    const string &range, string *eTag) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::InitiateMultipartUpload(
    const string &filePath, string *uploadId) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::UploadMultipart(
    const string &filePath, const string &uploadId, int partNumber,
    uint64_t contentLength, shared_ptr<std::iostream> buffer) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::CompleteMultipartUpload(
    const string &filePath, const string &uploadId,
    const std::vector<int> &sortedPartIds) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::AbortMultipartUpload(
    const string &filePath, const string &uploadId) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::UploadFile(
    const string &filePath, uint64_t fileSize,
    shared_ptr<std::iostream> buffer) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::SymLink(const string &filePath,
                                                const string &linkPath) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::ListDirectory(
    const string &dirPath, const shared_ptr<QS::Data::DirectoryTree> &dirTree) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::Stat(
    const string &path, const shared_ptr<QS::Data::DirectoryTree> &dirTree,
    time_t modifiedSince, bool *modified) {
  return GoodState();
}

ClientError<QSError::Value> NullClient::Statvfs(struct statvfs *stvfs) {
  return GoodState();
}

}  // namespace Client
}  // namespace QS
