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

#ifndef QSFS_FILESYSTEM_DRIVE_H_
#define QSFS_FILESYSTEM_DRIVE_H_

#include <sys/stat.h>
#include <sys/statvfs.h>

#include <string>
#include <utility>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/unordered_map.hpp"
#include "boost/weak_ptr.hpp"

#include "base/HashUtils.h"
#include "base/Singleton.hpp"
#include "data/Cache.h"
#include "data/DirectoryTree.h"

namespace QS {

namespace Client {
class Client;
class QSClient;
class QSTransferManager;
class TransferHandle;
class TransferManager;
}

namespace Data {
class DirectoryTree;
class Node;
}

namespace FileSystem {

typedef boost::unordered_map<std::string,
                             boost::shared_ptr<QS::Client::TransferHandle>,
                             HashUtils::StringHash>
    StringToTransferHandleMap;

class Drive : public Singleton<Drive> {
 public:
  ~Drive();

 public:
  bool IsMountable();

  // accessor
  const boost::shared_ptr<QS::Client::Client> &GetClient() const {
    return m_client;
  }
  const boost::shared_ptr<QS::Client::TransferManager> &GetTransferManager()
      const {
    return m_transferManager;
  }
  const boost::shared_ptr<QS::Data::Cache> &GetCache() const { return m_cache; }
  const boost::shared_ptr<QS::Data::DirectoryTree> &GetDirectoryTree() const {
    return m_directoryTree;
  }

  bool GetMountable() const {
    boost::lock_guard<boost::mutex> locker(m_mountableLock);
    return m_mountable;
  }
  bool GetCleanup() const {
    boost::lock_guard<boost::mutex> locker(m_cleanupLock);
    return m_cleanup;
  }

 public:
  // Connect to object storage
  //
  // @param  : void
  // @return : flag of success
  //
  // Notes: Connect will build up the root level of directory tree
  // asynchornizely
  bool Connect();

  // Return the drive root node.
  boost::shared_ptr<QS::Data::Node> GetRoot();

  // Get the node
  //
  // @param  : file path, flag force to update node, flag update if is dir, 
  //           flag update dir async
  // @return : a pair of { node, bool denotes if the node is modified comparing
  //           with the moment before this operation }
  //
  // Dir path should ending with '/'.
  // Using updateIfDirectory to invoke updating the directory tree
  // asynchronously if node is directory, which means the children of the
  // directory will be add to the tree.
  //
  // Notes: GetNode will connect to object storage to retrive the object and
  // update the local dir tree
  std::pair<boost::shared_ptr<QS::Data::Node>, bool> GetNode(
      const std::string &path, bool forceUpdateNode,
      bool updateIfDirectory = false, bool updateDirAsync = false);

  // Get the node from local dir tree
  // @param  : file path
  // @return : node
  //
  // GetNodeSimple just find the node in local dir tree
  boost::shared_ptr<QS::Data::Node> GetNodeSimple(const std::string &path);

  //
  //
  // Following APIs handle request from fuse, they
  // 1. Assume the path exists and arguments have been validated
  // 2. Assume the node has been exist in dir tree and synchornized with
  // object storage server.
  //
  //

  // Return information about the mounted bucket.
  struct statvfs GetFilesystemStatistics();

  // Find the children
  //
  // @param  : dir path, flag update if is dir
  // @return : child node list
  //
  // This will update the directory tree synchronizely if updateIfDir is true.
  std::vector<boost::weak_ptr<QS::Data::Node> > FindChildren(
      const std::string &dirPath, bool updateIfDir);

  // Change the permission bits of a file
  //
  // @param  : file path, mode
  // @return : void
  void Chmod(const std::string &filePath, mode_t mode);

  // Change the owner and group of a file
  //
  // @param  : file path, uid, gid
  // @return : void
  void Chown(const std::string &filePath, uid_t uid, gid_t gid);

  // Remove a file or an empty directory
  //
  // @param  : file path
  // @return : void
  void RemoveFile(const std::string &filePath, bool async = false);

  // Create a file
  //
  // @param  : file path, file mode, dev
  // @return : void
  //
  // MakeFile is called for creation of non-directory, non-symlink nodes.
  void MakeFile(const std::string &filePath, mode_t mode, dev_t dev = 0);

  // Create a Directory
  //
  // @param  : dir path, file mode
  // @return : void
  void MakeDir(const std::string &dirPath, mode_t mode);

  // Open a file
  //
  // @param  : file path, asynchronously download file if not loaded yet
  // @return : void
  void OpenFile(const std::string &filePath, bool async = false);

  // Read data from a file
  //
  // @param  : file path to read data from, offset, size, buf, flag doCheck
  // @return : number of bytes has been read
  //
  // If cannot find or file need update, download it, otherwise read from cache.
  // For download, if besides the size need to be download for this time, the
  // file has more data need to be download, in this case, an asynchronize task
  // will be submit to download extra partial data of the file.
  //
  // Flag doCheck control whether to check the file existence and file type.
  size_t ReadFile(const std::string &filePath, off_t offset, size_t size,
                  char *buf, bool async = false);

  // Read target of a symlink file
  //
  // @param  : link file path
  // @return : number of bytes has been read
  //
  // ReadSymlink read link file content which is the realitive path to the
  // target file, and update the symlink node in dir tree.
  void ReadSymlink(const std::string &linkPath);

  // Rename a file
  //
  // @param  : file path, new file path
  // @return : void
  void RenameFile(const std::string &filePath, const std::string &newFilePath);

  // Rename a directory
  //
  // @param  : dir path, new dir path, flag asynchornizely
  // @return : void
  void RenameDir(const std::string &dirPath, const std::string &newDirPath,
                 bool async = false);

  // Create a symbolic link to a file
  //
  // @param  : file path to link to, link path
  // @return : void
  //
  // symbolic link is a file that contains a reference to the file or dir,
  // the reference is the realitive path (from fuse) to the file,
  // fuse will parse . and .., so we just put the path as link file content.
  void SymLink(const std::string &filePath, const std::string &linkPath);

  // Truncate a file
  //
  // @param  : file path, new file size
  // @return : void
  void TruncateFile(const std::string &filePath, size_t newSize);

  // Upload a file
  //
  // @param  : file path
  // @return : void
  void UploadFile(const std::string &filePath, bool releaseFile,
                  bool updateMeta, bool async = false);

  // Release a file
  void ReleaseFile(const std::string &filePath);

  // Change access and modification times of a file
  //
  // @param  : file path, mtime
  // @return : void
  void Utimens(const std::string &path, time_t mtime);

  // Write a file
  //
  // @param  : file path to write data to, buf containing data, size, offset
  // @return : number of bytes has been wrote
  int WriteFile(const std::string &filePath, off_t offset, size_t size,
                const char *buf);

 private:
  // Download file contents
  //
  // @param  : file path, file content ranges, asynchronously or synchronizely
  // @return : void
  void DownloadFileContentRanges(const std::string &filePath,
                                 const QS::Data::ContentRangeDeque &ranges,
                                 time_t mtime, bool fileOpen,
                                 bool async = false);

  void DownloadFileContentRange(const std::string &filePath,
                                const std::pair<off_t, size_t> &range,
                                time_t mtime, bool fileOpen, bool async = false);

 private:
  boost::shared_ptr<QS::Client::Client> &GetClient() { return m_client; }
  boost::shared_ptr<QS::Client::TransferManager> &GetTransferManager() {
    return m_transferManager;
  }
  boost::shared_ptr<QS::Data::Cache> &GetCache() { return m_cache; }
  boost::shared_ptr<QS::Data::DirectoryTree> &GetDirectoryTree() {
    return m_directoryTree;
  }
  // mutator
  void SetClient(const boost::shared_ptr<QS::Client::Client> &client);
  void SetTransferManager(
      const boost::shared_ptr<QS::Client::TransferManager> &transferManager);
  void SetCache(const boost::shared_ptr<QS::Data::Cache> &cache);
  void SetDirectoryTree(
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree);
  void SetMountable(bool mountable) {
    boost::lock_guard<boost::mutex> locker(m_mountableLock);
    m_mountable = mountable;
  }
  void SetCleanup(bool cleanup) {
    boost::lock_guard<boost::mutex> locker(m_cleanupLock);
    m_cleanup = cleanup;
  }

 private:
  void CleanUp();
  void DoConnect();
  Drive();

  mutable boost::mutex m_mountableLock;
  bool m_mountable;

  mutable boost::mutex m_cleanupLock;
  bool m_cleanup;  // denote if drive get cleaned up

  bool m_connect;  // denote if drive connected to storage

  boost::shared_ptr<QS::Client::Client> m_client;
  boost::shared_ptr<QS::Client::TransferManager> m_transferManager;
  boost::shared_ptr<QS::Data::Cache> m_cache;
  boost::shared_ptr<QS::Data::DirectoryTree> m_directoryTree;
  StringToTransferHandleMap m_unfinishedMultipartUploadHandles;

  friend class Singleton<Drive>;
  friend class QS::Client::QSClient;
  friend class QS::Client::QSTransferManager;  // for cache
  friend void qsfs_destroy(void *userdata);
  friend struct UploadFileCallback;
};

}  // namespace FileSystem
}  // namespace QS

#endif  // QSFS_FILESYSTEM_DRIVE_H_
