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

#ifndef QSFS_CLIENT_CLIENT_H_
#define QSFS_CLIENT_CLIENT_H_

#include <stdint.h>
#include <sys/statvfs.h>
#include <time.h>

#include <iostream>
#include <string>
#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread_time.hpp"

#include "base/ThreadPool.h"
#include "client/ClientConfiguration.h"
#include "client/ClientFactory.h"
#include "client/QSError.h"
#include "client/RetryStrategy.h"

namespace QS {
namespace Data {
class Cache;
class DirectoryTree;
}  // namespace Data

namespace FileSystem {
class Drive;
}  // namespace FileSystem

namespace Client {

class ClientImpl;
class RetryStrategy;

class Client : private boost::noncopyable {
 public:
  Client(const boost::shared_ptr<ClientImpl> &impl =
             ClientFactory::Instance().MakeClientImpl(),
         const boost::shared_ptr<QS::Threading::ThreadPool> &executor =
             boost::shared_ptr<QS::Threading::ThreadPool>(
                 new QS::Threading::ThreadPool(
                     ClientConfiguration::Instance().GetPoolSize())),
         RetryStrategy retryStratety = GetCustomRetryStrategy());

  virtual ~Client();

 public:
  // Head bucket
  //
  // @param  : 
  // @return : ClientError
  virtual ClientError<QSError::Value> HeadBucket() = 0;

  // Delete a file
  //
  // @param  : file path, dir tree, cache
  // @return : ClientError
  //
  // DeleteFile is used to delete a file or an empty directory.
  // As object storage has no concept of file type (such as directory),
  // you can call DeleteFile to delete any object. But if the object is
  // a nonempty directory, DeleteFile will not delete its contents (including
  // files or subdirectories belongs to it).
  virtual ClientError<QSError::Value> DeleteFile(
      const std::string &filePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache) = 0;

  // Create an empty file
  //
  // @param  : file path
  // @return : ClientError
  virtual ClientError<QSError::Value> MakeFile(const std::string &filePath) = 0;

  // Create a directory
  //
  // @param  : dir path
  // @return : ClientError
  virtual ClientError<QSError::Value> MakeDirectory(
      const std::string &dirPath) = 0;

  // Move file
  //
  // @param  : file path, new file path, dir tree, cache
  // @return : ClientError
  //
  // MoveFile will invoke dirTree and Cache renaming.
  virtual ClientError<QSError::Value> MoveFile(
      const std::string &filePath, const std::string &newFilePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache) = 0;

  // Move directory
  //
  // @param  : source path, target path
  // @return : ClientError
  //
  // MoveDirectory will invoke dirTree and Cache renaming
  virtual ClientError<QSError::Value> MoveDirectory(
      const std::string &sourceDirPath, const std::string &targetDirPath,
      bool async = false) = 0;

  // Download file
  //
  // @param  : file path, contenct range, buffer(input), *eTag
  // @return : ClinetError
  //
  // If range is empty, then the whole file will be downloaded.
  // The file data will be written to buffer.
  virtual ClientError<QSError::Value> DownloadFile(
      const std::string &filePath,
      boost::shared_ptr<std::iostream> buffer,
      const std::string &range = std::string(), std::string *eTag = NULL) = 0;

  // Initiate multipart upload id
  //
  // @param  : file path, upload id (output)
  // @return : ClientEror
  virtual ClientError<QSError::Value> InitiateMultipartUpload(
      const std::string &filePath, std::string *uploadId) = 0;

  // Upload multipart
  //
  // @param  : file path, upload id, part number, content len, buffer
  // @return : ClientError
  virtual ClientError<QSError::Value> UploadMultipart(
      const std::string &filePath, const std::string &uploadId, int partNumber,
      uint64_t contentLength,
      boost::shared_ptr<std::iostream> buffer) = 0;

  // Complete multipart upload
  //
  // @param  : file path, upload id, sorted part ids
  // @return : ClientError
  virtual ClientError<QSError::Value> CompleteMultipartUpload(
      const std::string &filePath, const std::string &uploadId,
      const std::vector<int> &sortedPartIds) = 0;

  // Abort mulitpart upload
  //
  // @param  : file path, upload id
  // @return : ClientError
  virtual ClientError<QSError::Value> AbortMultipartUpload(
      const std::string &filePath, const std::string &uploadId) = 0;

  // Upload file using PutObject
  //
  // @param  : file path, file size, buffer
  // @return : ClientError
  virtual ClientError<QSError::Value> UploadFile(
      const std::string &filePath, uint64_t fileSize,
      boost::shared_ptr<std::iostream> buffer) = 0;

  // Create a symbolic link to a file
  //
  // @param  : file path to link to, link path
  // @return : void
  //
  // symbolic link is a file that contains a reference to the file or dir,
  // the reference is the realitive path (from fuse) to the file,
  // fuse will parse . and .., so we just put the path as link file content.
  virtual ClientError<QSError::Value> SymLink(const std::string &filePath,
                                              const std::string &linkPath) = 0;

  // List directory
  //
  // @param  : dir path,dir tree
  // @return : ClientError
  //
  // ListDirectory will update directory in tree if dir exists and is modified
  // or grow the tree if the directory is not existing in tree.
  //
  // Notice the dirPath should end with delimiter.
  virtual ClientError<QSError::Value> ListDirectory(
      const std::string &dirPath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree) = 0;

  // Get object meta data
  //
  // @param  : file path, dir tree, modifiedSince, *modified(output)
  // @return : ClientError
  //
  // Using modifiedSince to match if the object modified since then.
  // Using modifiedSince = 0 to always get object meta data, this is default.
  // Using modified to gain output of object modified status since given time.
  //
  // Stat will update the dir tree if the node is modified
  virtual ClientError<QSError::Value> Stat(
      const std::string &path,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      time_t modifiedSince = 0, bool *modified = NULL) = 0;

  // Get information about mounted bucket
  //
  // @param  : stvfs(output)
  // @return : ClientError
  virtual ClientError<QSError::Value> Statvfs(struct statvfs *stvfs) = 0;

 public:
  void RetryRequestSleep(boost::posix_time::milliseconds sleepTime) const;

  const RetryStrategy &GetRetryStrategy() const { return m_retryStrategy; }
  const boost::shared_ptr<ClientImpl> &GetClientImpl() const { return m_impl; }

 protected:
  boost::shared_ptr<ClientImpl> GetClientImpl() { return m_impl; }
  const boost::shared_ptr<QS::Threading::ThreadPool> &GetExecutor() const {
    return m_executor;
  }

 private:
  boost::shared_ptr<ClientImpl> m_impl;
  boost::shared_ptr<QS::Threading::ThreadPool> m_executor;
  RetryStrategy m_retryStrategy;
  mutable boost::mutex m_retryLock;
  mutable boost::condition_variable m_retrySignal;

  friend class QS::FileSystem::Drive;
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_CLIENT_H_
