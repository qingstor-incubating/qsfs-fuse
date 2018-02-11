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

#ifndef QSFS_CLIENT_QSCLIENT_H_
#define QSFS_CLIENT_QSCLIENT_H_

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "qingstor/QingStor.h"

#include "client/Client.h"


namespace QingStor {
class QsConfig;
class SDKOptions;
}  // namespace QingStor

namespace QS {

namespace Data {
class Cache;
class DirectoryTree;
}  // namespace Data

namespace Client {

class QSClientImpl;

class QSClient : public Client {
 public:
  QSClient();
  ~QSClient();

 public:
  // Head bucket
  //
  // @param  : 
  // @return : ClientError
  ClientError<QSError::Value> HeadBucket();

  // Delete a file
  //
  // @param  : file path, diretory tree, cache
  // @return : ClientError
  //
  // DeleteFile is used to delete a file or an empty directory.
  // As object storage has no concept of file type (such as directory),
  // you can call DeleteFile to delete any object. But if the object is
  // a nonempty directory, DeleteFile will not delete its contents (including
  // files or subdirectories belongs to it).
  ClientError<QSError::Value> DeleteFile(
      const std::string &filePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);

  // Create an empty file
  //
  // @param  : file path
  // @return : ClientError
  // As qs sdk doesn't return the created file meta data in PutObjectOutput,
  // So we cannot grow the directory tree here, instead we need to call
  // Stat to head the object again in Drive::MakeFile;
  ClientError<QSError::Value> MakeFile(const std::string &filePath);

  // Create a directory
  //
  // @param  : dir path
  // @return : ClientError
  // As qs sdk doesn't return the created dir meta data in PutObjectOutput,
  // So we cannot grow the directory tree here, instead we need to call
  // Stat to head the object again in Drive::MakeDirectory;
  ClientError<QSError::Value> MakeDirectory(const std::string &dirPath);

  // Move file
  //
  // @param  : file path, new file path, directory tree, cache
  // @return : ClientError
  //
  // MoveFile will invoke dirTree and Cache renaming.
  ClientError<QSError::Value> MoveFile(
      const std::string &sourcefilePath, const std::string &destFilePath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);

  // Move directory
  //
  // @param  : source path, target path
  // @return : ClientError
  //
  // MoveDirectory move dir, subdirs and subfiles recursively.
  // Notes: MoveDirectory will do nothing on dir tree and cache.
  ClientError<QSError::Value> MoveDirectory(const std::string &sourceDirPath,
                                            const std::string &targetDirPath,
                                            bool async = false);

  // Download file
  //
  // @param  : file path, buffer(input), contenct range, eTag (output)
  // @return : ClinetError
  //
  // If range is empty, then the whole file will be downloaded.
  // The file data will be written to buffer.
  ClientError<QSError::Value> DownloadFile(
      const std::string &filePath,
      boost::shared_ptr<std::iostream> buffer,
      const std::string &range = std::string(), std::string *eTag = NULL);

  // Initiate multipart upload id
  //
  // @param  : file path, upload id (output)
  // @return : ClientError
  ClientError<QSError::Value> InitiateMultipartUpload(
      const std::string &filePath, std::string *uploadId);

  // Upload multipart
  //
  // @param  : file path, upload id, part number, content len, buffer
  // @return : ClientError
  ClientError<QSError::Value> UploadMultipart(
      const std::string &filePath, const std::string &uploadId, int partNumber,
      uint64_t contentLength, boost::shared_ptr<std::iostream> buffer);

  // Complete multipart upload
  //
  // @param  : file path, upload id, sorted part ids
  // @return : ClientError
  ClientError<QSError::Value> CompleteMultipartUpload(
      const std::string &filePath, const std::string &uploadId,
      const std::vector<int> &sortedPartIds);

  // Abort mulitpart upload
  //
  // @param  : file path, upload id
  // @return : ClientError
  ClientError<QSError::Value> AbortMultipartUpload(const std::string &filePath,
                                                   const std::string &uploadId);

  // Upload file using PutObject
  //
  // @param  : file path, file size, buffer
  // @return : ClientError
  ClientError<QSError::Value> UploadFile(
      const std::string &filePath, uint64_t fileSize,
      boost::shared_ptr<std::iostream> buffer);

  // Create a symbolic link to a file
  //
  // @param  : file path to link to, link path
  // @return : void
  //
  // symbolic link is a file that contains a reference to the file or dir,
  // the reference is the realitive path (from fuse) to the file,
  // fuse will parse . and .., so we just put the path as link file content.
  ClientError<QSError::Value> SymLink(const std::string &filePath,
                                      const std::string &linkPath);

  // List directory
  //
  // @param  : dir path, directory tree
  // @return : ClientError
  //
  // ListDirectory will update directory in tree if dir exists and is modified
  // or grow the tree if the directory is not existing in tree.
  //
  // Notice the dirPath should end with delimiter.
  ClientError<QSError::Value> ListDirectory(
      const std::string &dirPath,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree);

  // Get object meta data
  //
  // @param  : file path, directory tree, modifiedSince, *modified(output)
  // @return : ClientError
  //
  // Using modifiedSince to match if the object modified since then.
  // Using modifiedSince = 0 to always get object meta data, this is default.
  // Using modified to gain output of object modified status since given time.
  //
  // Stat will update the node meta in dir tree if the node is modified.
  //
  // Notes: the meta will be return if object is modified, otherwise
  // the response code will be 304 (NOT MODIFIED) and no meta returned.
  ClientError<QSError::Value> Stat(
      const std::string &path,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      time_t modifiedSince = 0, bool *modified = NULL);

  // Get information about mounted bucket
  //
  // @param  : *stvfs(output)
  // @return : ClientError
  ClientError<QSError::Value> Statvfs(struct statvfs *stvfs);

 public:
  //
  // Following api only submit sdk request, no ops on local dirtree and cache.
  //

  // Move object
  //
  // @param  : source file path, target file path
  // @return : ClientError
  //
  // This only submit sdk put(move) object request, no ops on dir tree and cache
  ClientError<QSError::Value> MoveObject(const std::string &sourcePath,
                                         const std::string &targetPath);

 public:
  static const boost::shared_ptr<QingStor::QsConfig> &GetQingStorConfig();
  const boost::shared_ptr<QSClientImpl> &GetQSClientImpl() const;

 private:
  boost::shared_ptr<QSClientImpl> &GetQSClientImpl();
  void DoGetQSClientImpl();
  static void StartQSService();
  static void DoStartQSService();
  void CloseQSService();
  void InitializeClientImpl();

 private:
  static QingStor::SDKOptions m_sdkOptions;
  static boost::shared_ptr<QingStor::QsConfig> m_qingStorConfig;
  boost::shared_ptr<QSClientImpl> m_qsClientImpl;
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_QSCLIENT_H_
