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

#include "filesystem/Drive.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <deque>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "boost/bind.hpp"
#include "boost/exception/to_string.hpp"
#include "boost/foreach.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread.hpp"
#include "boost/thread/once.hpp"
#include "boost/weak_ptr.hpp"

#include "base/Exception.h"
#include "base/LogMacros.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/TimeUtils.h"
#include "base/Utils.h"
#include "base/UtilsWithLog.h"
#include "client/Client.h"
#include "client/ClientConfiguration.h"
#include "client/ClientFactory.h"
#include "client/QSError.h"
#include "client/TransferHandle.h"
#include "client/TransferManager.h"
#include "client/TransferManagerFactory.h"
#include "configure/Default.h"
#include "configure/Options.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/FileMetaData.h"
#include "data/FileMetaDataManager.h"
#include "data/IOStream.h"
#include "data/Node.h"

namespace QS {

namespace FileSystem {

using boost::bind;
using boost::make_shared;
using boost::shared_ptr;
using boost::to_string;
using boost::weak_ptr;
using QS::Client::Client;
using QS::Client::ClientError;
using QS::Client::ClientFactory;
using QS::Client::GetMessageForQSError;
using QS::Client::IsGoodQSError;
using QS::Client::QSError;
using QS::Client::TransferHandle;
using QS::Client::TransferManager;
using QS::Client::TransferManagerConfigure;
using QS::Client::TransferManagerFactory;
using QS::Data::Cache;
using QS::Data::ContentRangeDeque;
using QS::Data::ChildrenMultiMapConstIterator;
using QS::Data::DirectoryTree;
using QS::Data::Entry;
using QS::Data::FileMetaData;
using QS::Data::FileType;
using QS::Data::FilePathToNodeUnorderedMap;
using QS::Data::IOStream;
using QS::Data::Node;
using QS::Exception::QSException;
using QS::StringUtils::FormatPath;
using QS::Utils::AppendPathDelim;
using QS::UtilsWithLog::DeleteFilesInDirectory;
using QS::UtilsWithLog::IsDirectory;
using QS::Utils::GetDirName;
using QS::Utils::GetProcessEffectiveUserID;
using QS::Utils::GetProcessEffectiveGroupID;
using QS::Utils::IsRootDirectory;
using std::deque;
using std::make_pair;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

static boost::once_flag connectOnceFalg = BOOST_ONCE_INIT;

// --------------------------------------------------------------------------
struct PrintErrorMsg {
  PrintErrorMsg() {}
  void operator()(const ClientError<QSError::Value> &err) {
    DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  }
};

// --------------------------------------------------------------------------
Drive::Drive()
    : m_mountable(true),
      m_cleanup(false),
      m_connect(false),
      m_client(ClientFactory::Instance().MakeClient()),
      m_transferManager(
          TransferManagerFactory::Create(TransferManagerConfigure())) {
  QS::Configure::Options &options = QS::Configure::Options::Instance();
  uint64_t cacheSize =
      static_cast<uint64_t>(options.GetMaxCacheSizeInMB() * QS::Size::MB1);
  m_cache = make_shared<Cache>(cacheSize);

  uid_t uid =
      options.IsOverrideUID() ? options.GetUID() : GetProcessEffectiveUserID();
  gid_t gid =
      options.IsOverrideGID() ? options.GetGID() : GetProcessEffectiveGroupID();
  mode_t mode = options.IsAllowOther() ? (options.IsUmaskMountPoint()
                                              ? ((S_IRWXU | S_IRWXG | S_IRWXO) &
                                                 ~options.GetUmaskMountPoint())
                                              : (S_IRWXU | S_IRWXG | S_IRWXO))
                                       : S_IRWXU;
  m_directoryTree = make_shared<DirectoryTree>(time(NULL), uid, gid, mode);

  m_transferManager->SetClient(m_client);

  QS::Data::FileMetaDataManager::Instance().SetDirectoryTree(
      m_directoryTree.get());
}

// --------------------------------------------------------------------------
Drive::~Drive() { CleanUp(); }

// --------------------------------------------------------------------------
void Drive::CleanUp() {
  if (!GetCleanup()) {
    // abort unfinished multipart uploads
    if (!m_unfinishedMultipartUploadHandles.empty()) {
      BOOST_FOREACH (StringToTransferHandleMap::value_type &p,
                     m_unfinishedMultipartUploadHandles) {
        m_transferManager->AbortMultipartUpload(p.second);
      }
    }
    // remove disk cache folder if existing
    string diskfolder =
        QS::Configure::Options::Instance().GetDiskCacheDirectory();
    if (QS::Utils::FileExists(diskfolder) && IsDirectory(diskfolder)) {
      DeleteFilesInDirectory(diskfolder, true);  // delete folder itself
    }

    m_client.reset();
    m_transferManager.reset();
    m_cache.reset();
    m_directoryTree.reset();
    m_unfinishedMultipartUploadHandles.clear();

    SetCleanup(true);
  }
}

// --------------------------------------------------------------------------
void Drive::SetClient(const shared_ptr<Client> &client) { m_client = client; }

// --------------------------------------------------------------------------
void Drive::SetTransferManager(
    const shared_ptr<TransferManager> &transferManager) {
  m_transferManager = transferManager;
}

// --------------------------------------------------------------------------
void Drive::SetCache(const shared_ptr<Cache> &cache) { m_cache = cache; }

// --------------------------------------------------------------------------
void Drive::SetDirectoryTree(const shared_ptr<DirectoryTree> &dirTree) {
  m_directoryTree = dirTree;
}

// --------------------------------------------------------------------------
bool Drive::IsMountable() {
  SetMountable(Connect());
  return GetMountable();
}

// --------------------------------------------------------------------------
void DoListRootDirectory(shared_ptr<Client> client,
                         shared_ptr<DirectoryTree> dirTree) {
  ClientError<QSError::Value> err = client->ListDirectory("/", dirTree);
  DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
}
// --------------------------------------------------------------------------
void Drive::DoConnect() {
  ClientError<QSError::Value> err = GetClient()->HeadBucket();
  if (!IsGoodQSError(err)) {
    Error(GetMessageForQSError(err));
    m_connect = false;
    return;
  } else {
    m_connect = true;
  }

  // Update root node of the tree
  if (!m_directoryTree->GetRoot()) {
    m_directoryTree->Grow(QS::Data::BuildDefaultDirectoryMeta("/", time(NULL)));
  }

  // Build up the root level of directory tree asynchornizely.
  boost::thread(
      bind(boost::type<void>(), DoListRootDirectory, m_client, m_directoryTree))
      .detach();
}

// --------------------------------------------------------------------------
bool Drive::Connect() {
  boost::call_once(connectOnceFalg,
                   bind(boost::type<void>(), &Drive::DoConnect, this));
  return m_connect;
}

// --------------------------------------------------------------------------
shared_ptr<Node> Drive::GetRoot() { return m_directoryTree->GetRoot(); }

// --------------------------------------------------------------------------
pair<shared_ptr<Node>, bool> Drive::GetNode(const string &path,
                                            bool forceUpdateNode,
                                            bool updateIfDirectory,
                                            bool updateDirAsync) {
  if (path.empty()) {
    Error("Null file path");
    return make_pair(shared_ptr<Node>(), false);
  }
  shared_ptr<Node> node = m_directoryTree->Find(path);
  bool modified = false;
  int32_t expireDurationInMin =
      QS::Configure::Options::Instance().GetStatExpireInMin();
  if (node && *node) {
    if (QS::TimeUtils::IsExpire(node->GetCachedTime(), expireDurationInMin) ||
        forceUpdateNode) {
      // Update Node
      time_t modifiedSince = 0;
      modifiedSince = node->GetMTime();
      ClientError<QSError::Value> err =
          GetClient()->Stat(path, m_directoryTree, modifiedSince, &modified);
      if (!IsGoodQSError(err)) {
        // As user can remove file through other ways such as web console, etc.
        // So we need to remove file from local dir tree and cache.
        if (err.GetError() == QSError::NOT_FOUND) {
          // remove node
          DebugInfo("File not exist " + FormatPath(path));
          m_directoryTree->Remove(path);
          if (m_cache->HasFile(path)) {
            m_cache->Erase(path);
          }
        } else {
          Error(GetMessageForQSError(err));
        }
      }
    }
  } else {
    ClientError<QSError::Value> err =
        GetClient()->Stat(path, m_directoryTree);  // head it
    if (IsGoodQSError(err)) {
      node = m_directoryTree->Find(path);
    } else {
      if (err.GetError() == QSError::NOT_FOUND) {
        DebugInfo("File not exist " + FormatPath(path));
      } else {
        Error(GetMessageForQSError(err));
      }
    }
  }

  // Update directory tree asynchornizely
  // Should check node existence as given file could be not existing which is
  // not be considered as an error.
  // The modified time is only the meta of an object, we should not take
  // modified time as an precondition to decide if we need to update dir or not.
  if (node && *node && node->IsDirectory() && updateIfDirectory &&
      (QS::TimeUtils::IsExpire(node->GetCachedTime(), expireDurationInMin) ||
       forceUpdateNode)) {
    PrintErrorMsg receivedHandler;
    string path_ = AppendPathDelim(path);
    if (updateDirAsync) {
      GetClient()->GetExecutor()->SubmitAsync(
          bind(boost::type<void>(), receivedHandler, _1),
          bind(boost::type<ClientError<QSError::Value> >(),
               &QS::Client::Client::ListDirectory, m_client.get(), _1, _2),
          path_, m_directoryTree);
    } else {
      receivedHandler(GetClient()->ListDirectory(path_, m_directoryTree));
    }
  }

  return make_pair(node, modified);
}

// --------------------------------------------------------------------------
shared_ptr<Node> Drive::GetNodeSimple(const string &path) {
  return m_directoryTree->Find(path);
}

// --------------------------------------------------------------------------
struct statvfs Drive::GetFilesystemStatistics() {
  struct statvfs statv;
  ClientError<QSError::Value> err = GetClient()->Statvfs(&statv);
  ErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  return statv;
}

// --------------------------------------------------------------------------
vector<weak_ptr<Node> > Drive::FindChildren(const string &dirPath,
                                            bool updateIfDir) {
  shared_ptr<Node> node = GetNodeSimple(dirPath);
  if (node && *node) {
    if (node->IsDirectory() && updateIfDir) {
      // Update directory tree synchronously
      ClientError<QSError::Value> err =
          GetClient()->ListDirectory(dirPath, m_directoryTree);
      DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
    }
    return m_directoryTree->FindChildren(dirPath);
  } else {
    DebugError("Directory not exist " + FormatPath(dirPath));
    return vector<weak_ptr<Node> >();
  }
}

// --------------------------------------------------------------------------
void Drive::Chmod(const std::string &filePath, mode_t mode) {
  Warning("chmod not supported");
  // TODO(jim): wait for sdk api of meta data
  // change meta mode: x-qs-meta-mode
  // call Stat to update meta locally
}

// --------------------------------------------------------------------------
void Drive::Chown(const std::string &filePath, uid_t uid, gid_t gid) {
  Warning("chown not supported");
  // TODO(jim): wait for sdk api of meta
  // change meta uid gid; x-qs-meta-uid, x-qs-meta-gid
  // call Stat to update meta locally
}

// --------------------------------------------------------------------------
struct PrintMsgForDeleteFile {
  string filePath;
  explicit PrintMsgForDeleteFile(const string &path) : filePath(path) {}

  void operator()(const ClientError<QSError::Value> &err) {
    if (IsGoodQSError(err)) {
      Info("Delete file " + FormatPath(filePath));
    } else {
      Error(GetMessageForQSError(err));
    }
  }
};
// --------------------------------------------------------------------------
// Remove a file or an empty directory
void Drive::RemoveFile(const string &filePath, bool async) {
  PrintMsgForDeleteFile receivedHandler(filePath);

  if (async) {  // delete file asynchronously
    GetClient()->GetExecutor()->SubmitAsyncPrioritized(
        bind(boost::type<void>(), receivedHandler, _1),
        bind(boost::type<ClientError<QSError::Value> >(),
             &QS::Client::Client::DeleteFile, m_client.get(), _1, _2, _3),
        filePath, m_directoryTree, m_cache);
  } else {
    receivedHandler(
        GetClient()->DeleteFile(filePath, m_directoryTree, m_cache));
  }
}

// --------------------------------------------------------------------------
void Drive::MakeFile(const string &filePath, mode_t mode, dev_t dev) {
  FileType::Value type = FileType::File;
  if (mode & S_IFREG) {
    type = FileType::File;
  } else if (mode & S_IFBLK) {
    type = FileType::Block;
  } else if (mode & S_IFCHR) {
    type = FileType::Character;
  } else if (mode & S_IFIFO) {
    type = FileType::FIFO;
  } else if (mode & S_IFSOCK) {
    type = FileType::Socket;
  } else {
    Warning(
        "Try to create a directory or symbolic link, but MakeFile is only for "
        "creation of non-directory and non-symlink nodes. ");
    return;
  }

  if (type == FileType::File) {
    ClientError<QSError::Value> err = GetClient()->MakeFile(filePath);
    if (!IsGoodQSError(err)) {
      Error(GetMessageForQSError(err));
      return;
    }

    Info("Create file " + FormatPath(filePath));

    // QSClient::MakeFile doesn't update directory tree (refer it for details)
    // with the created file node, So we call Stat synchronizely.
    err = GetClient()->Stat(filePath, m_directoryTree);
    DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  } else {
    Error("Not support to create a special file (block, char, FIFO, etc.)");
    // This only make file of other types in local dir tree, nothing happens
    // in server. And it will be removed when synchronize with server.
    // TODO(jim): may consider to support them in server if sdk support this
    // time_t mtime = time(NULL);
    // m_directoryTree->Grow(make_shared<FileMetaData>(
    //     filePath, 0, mtime, mtime, GetProcessEffectiveUserID(),
    //     GetProcessEffectiveGroupID(), mode, type, "", "", false, dev));
  }
}

// --------------------------------------------------------------------------
void Drive::MakeDir(const string &dirPath, mode_t mode) {
  ClientError<QSError::Value> err = GetClient()->MakeDirectory(dirPath);
  if (!IsGoodQSError(err)) {
    Error(GetMessageForQSError(err));
    return;
  }

  Info("Create directory " + FormatPath(dirPath));

  // QSClient::MakeDirectory doesn't grow directory tree with the created dir
  // node, So we call Stat synchronizely.
  err = GetClient()->Stat(dirPath, m_directoryTree);
  DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
}

// --------------------------------------------------------------------------
void Drive::OpenFile(const string &filePath, bool async) {
  pair<shared_ptr<Node>, bool> res = GetNode(filePath, true, false, false);
  shared_ptr<Node> node = res.first;
  bool modified = res.second;

  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return;
  }

  Info("Open file " + FormatPath(filePath));

  uint64_t fileSize = node->GetFileSize();
  time_t mtime = node->GetMTime();
  bool isOpen = node->IsFileOpen();
  assert(fileSize >= 0);
  if (fileSize == 0) {
    m_cache->Write(filePath, 0, 0, NULL, mtime, isOpen);
  } else if (fileSize > 0) {
    bool fileContentExist = m_cache->HasFileData(filePath, 0, fileSize);
    if (!fileContentExist || modified) {
      QS::Data::ContentRangeDeque ranges =
          m_cache->GetUnloadedRanges(filePath, 0, fileSize);
      if (!ranges.empty()) {
        DownloadFileContentRanges(filePath, ranges, mtime, isOpen, async);
      }
    }
  }

  node->SetFileOpen(true);
}

// --------------------------------------------------------------------------
size_t Drive::ReadFile(const string &filePath, off_t offset, size_t size,
                       char *buf, bool async) {
  // read is only called if the file has been opend with the correct flags
  // no need to head it for latest meta
  shared_ptr<Node> node = GetNodeSimple(filePath);
  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return 0;
  }

  // Ajust size or calculate remaining size
  uint64_t downloadSize = size;
  int64_t remainingSize = 0;
  uint64_t fileSize = node->GetFileSize();
  if (offset + size > fileSize) {
    DebugInfo("Read file [offset:size=" + to_string(offset) + ":" +
              to_string(size) + " file size=" + to_string(fileSize) + "] " +
              FormatPath(filePath) + " Overflow, ajust it");
    downloadSize = fileSize - offset;
  } else {
    remainingSize = fileSize - (offset + size);
  }

  if (downloadSize == 0) {
    return 0;
  }

  time_t mtime = node->GetMTime();
  bool isOpen = node->IsFileOpen();
  if (mtime > m_cache->GetTime(filePath)) {
    m_cache->Erase(filePath);
  }
  // Download file if not found in cache or if cache need update
  bool fileContentExist = m_cache->HasFileData(filePath, offset, downloadSize);
  if (!fileContentExist) {
    // download synchronizely for request file part
    shared_ptr<IOStream> stream = make_shared<IOStream>(downloadSize);
    shared_ptr<TransferHandle> handle =
        m_transferManager->DownloadFile(filePath, offset, downloadSize, stream);

    // waiting for download to finish for request file part
    if (handle) {
      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        Info("Download file [offset:len=" + to_string(offset) + ":" +
             to_string(downloadSize) + "] " + FormatPath(filePath));

        bool success = m_cache->Write(filePath, offset, downloadSize, stream,
                                      mtime, isOpen);
        DebugErrorIf(!success,
                     "Fail to write cache [offset:len=" + to_string(offset) +
                         ":" + to_string(downloadSize) + "] " +
                         FormatPath(filePath));
      }
    }
  }

  // download unloaded part
  if (remainingSize > 0) {
    ContentRangeDeque ranges =
        m_cache->GetUnloadedRanges(filePath, 0, fileSize);
    if (!ranges.empty()) {
      DownloadFileContentRanges(filePath, ranges, mtime, isOpen, async);
    }
  }

  // Read from cache
  pair<size_t, ContentRangeDeque> outcome =
      m_cache->Read(filePath, offset, downloadSize, buf, mtime);
  return outcome.first;
}

// --------------------------------------------------------------------------
void Drive::ReadSymlink(const std::string &linkPath) {
  shared_ptr<Node> node = GetNodeSimple(linkPath);
  shared_ptr<stringstream> buffer = make_shared<stringstream>();
  ClientError<QSError::Value> err = GetClient()->DownloadFile(linkPath, buffer);
  if (IsGoodQSError(err)) {
    node->SetSymbolicLink(buffer->str());
  } else {
    Error(GetMessageForQSError(err));
  }
}

// --------------------------------------------------------------------------
void Drive::RenameFile(const string &filePath, const string &newFilePath) {
  // Do Renaming
  ClientError<QSError::Value> err =
      GetClient()->MoveFile(filePath, newFilePath, m_directoryTree, m_cache);

  // Update meta(such as mtime, .etc)
  if (IsGoodQSError(err)) {
    pair<shared_ptr<Node>, bool> res = GetNode(newFilePath, true, false, false);
    shared_ptr<Node> node = res.first;
    if (node && *node) {
      Info("Rename file " + FormatPath(filePath, newFilePath));
    } else {
      Warning("Fail to rename file " + FormatPath(filePath, newFilePath));
    }
    return;
  } else {
    Error(GetMessageForQSError(err));
    return;
  }
}

// --------------------------------------------------------------------------
struct RenameDirCallback {
  string dirPath;
  string newDirPath;
  shared_ptr<DirectoryTree> dirTree;
  shared_ptr<Cache> cache;
  Drive *drive;

  RenameDirCallback(const string &dir, const string &newdir,
                    const shared_ptr<DirectoryTree> &dirtree_,
                    const shared_ptr<Cache> &cache_, Drive *drive_)
      : dirPath(dir),
        newDirPath(newdir),
        dirTree(dirtree_),
        cache(cache_),
        drive(drive_) {}

  void operator()(const ClientError<QSError::Value> &err) {
    if (IsGoodQSError(err)) {
      // Rename local cache
      shared_ptr<Node> node = dirTree->Find(dirPath);
      deque<string> childPaths = node->GetChildrenIdsRecursively();
      size_t len = dirPath.size();
      deque<string> childTargetPaths;
      BOOST_FOREACH (const string &path, childPaths) {
        if (path.substr(0, len) != dirPath) {
          DebugError("Directory has an invalid child file [path=" + dirPath +
                     " child=" + path + "]");
          childTargetPaths.push_back(path);  // put old path
        }
        childTargetPaths.push_back(newDirPath + path.substr(len));
      }
      while (!childPaths.empty() && !childTargetPaths.empty()) {
        string source = childPaths.back();
        childPaths.pop_back();
        string target = childTargetPaths.back();
        childTargetPaths.pop_back();
        if (cache && cache->HasFile(source)) {
          cache->Rename(source, target);
        }
      }

      // Remove old dir node from dir tree
      if (dirTree) {
        dirTree->Remove(dirPath);
      }

      // Add new dir node to dir tree
      if (drive) {
        pair<shared_ptr<Node>, bool> res =
            drive->GetNode(newDirPath, true, true, false);
        node = res.first;
        if (node && *node) {
          Info("Rename directory " + FormatPath(dirPath, newDirPath));
        } else {
          Warning("Fail to rename directory " + FormatPath(dirPath));
        }
      }
    } else {
      Error(GetMessageForQSError(err));
    }
  }
};

// --------------------------------------------------------------------------
void Drive::RenameDir(const string &dirPath, const string &newDirPath,
                      bool async) {
  // Do Renaming
  RenameDirCallback receivedHandler(dirPath, newDirPath, m_directoryTree,
                                    m_cache, this);

  // When submit asynchronize task, and task itself should be run
  // in threadpool synchronizely.
  // So invoke Client::MoveDirectory with async=false
  if (async) {
    GetClient()->GetExecutor()->SubmitAsyncPrioritized(
        bind(boost::type<void>(), receivedHandler, _1),
        bind(boost::type<ClientError<QSError::Value> >(),
             &QS::Client::Client::MoveDirectory, m_client.get(), _1, _2, false),
        dirPath, newDirPath);
  } else {
    receivedHandler(GetClient()->MoveDirectory(dirPath, newDirPath, false));
  }
}

// --------------------------------------------------------------------------
// Symbolic link is a file that contains a reference to another file or dir
// in the form of an absolute path (in qsfs) or relative path and that affects
// pathname resolution.
void Drive::SymLink(const string &filePath, const string &linkPath) {
  assert(!filePath.empty() && !linkPath.empty());
  ClientError<QSError::Value> err = GetClient()->SymLink(filePath, linkPath);
  if (!IsGoodQSError(err)) {
    Error("Fail to create a symbolic link [path=" + filePath +
          ", link=" + linkPath);
    Error(GetMessageForQSError(err));
    return;
  }

  Info("Create symlink " + FormatPath(filePath, linkPath));

  // QSClient::Symlink doesn't update directory tree (refer it for details)
  // with the created symlink node, So we call Stat synchronizely.
  err = GetClient()->Stat(linkPath, m_directoryTree);
  if (!IsGoodQSError(err)) {
    Error(GetMessageForQSError(err));
    return;
  }

  shared_ptr<Node> lnkNode = GetNodeSimple(linkPath);
  if (lnkNode && *lnkNode) {
    lnkNode->SetSymbolicLink(filePath);
  }
}

// --------------------------------------------------------------------------
void Drive::TruncateFile(const string &filePath, size_t newSize) {
  shared_ptr<Node> node = GetNodeSimple(filePath);
  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return;
  }

  if (newSize != node->GetFileSize()) {
    Info("Truncate file [oldsize:newsize=" + to_string(node->GetFileSize()) +
         ":" + to_string(newSize) + "]" + FormatPath(filePath));
    m_cache->Resize(filePath, newSize, node->GetMTime());
    node->SetFileSize(newSize);
    node->SetNeedUpload(true);
  }
}

// --------------------------------------------------------------------------
struct UploadFileCallback {
  string filePath;
  shared_ptr<Node> node;
  shared_ptr<Cache> cache;
  shared_ptr<Client> client;
  shared_ptr<DirectoryTree> dirTree;
  Drive *drive;
  bool releaseFile;
  bool updateMeta;

  UploadFileCallback(const string &filePath_, const shared_ptr<Node> &node_,
                     const shared_ptr<Cache> &cache_,
                     const shared_ptr<Client> &client_,
                     const shared_ptr<DirectoryTree> &dirTree_, Drive *drive_,
                     bool releaseFile_, bool updateMeta_)
      : filePath(filePath_),
        node(node_),
        cache(cache_),
        client(client_),
        dirTree(dirTree_),
        drive(drive_),
        releaseFile(releaseFile_),
        updateMeta(updateMeta_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle && cache && client && drive) {
      node->SetNeedUpload(false);
      if (releaseFile) {
        node->SetFileOpen(false);
        cache->SetFileOpen(filePath, false);
        Info("Close file " + FormatPath(filePath));
      }
      if (handle->IsMultipart()) {
        drive->m_unfinishedMultipartUploadHandles.emplace(
            handle->GetObjectKey(), handle);
      }
      handle->WaitUntilFinished();
      drive->m_unfinishedMultipartUploadHandles.erase(handle->GetObjectKey());

      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        Info("Done Upload file " + FormatPath(filePath));
        // update meta mtime
        if (updateMeta) {
          ClientError<QSError::Value> err =
              client->Stat(handle->GetObjectKey(), dirTree);
          if (IsGoodQSError(err)) {
            // update cache mtime
            shared_ptr<Node> node1 =
                drive->GetNodeSimple(handle->GetObjectKey());
            if (node1 && *node1) {
              cache->SetTime(handle->GetObjectKey(), node1->GetMTime());
            }
          } else {
            Error(GetMessageForQSError(err));
          }
        }  // update Meta
      }    // Done Transfer
    }
  }
};

// --------------------------------------------------------------------------
void Drive::UploadFile(const string &filePath, bool releaseFile,
                       bool updateMeta, bool async) {
  Info("Start upload file " + FormatPath(filePath));
  // upload should just get file node from local
  shared_ptr<Node> node = GetNodeSimple(filePath);

  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return;
  }

  uint64_t fileSize = node->GetFileSize();
  time_t mtime = node->GetMTime();
  ContentRangeDeque ranges = m_cache->GetUnloadedRanges(filePath, 0, fileSize);
  // download unloaded pages for file
  // this is need as user could open a file and edit a part of it,
  // but you need the completed file in order to upload it.
  if (!ranges.empty()) {
    bool fileOpen = node->IsFileOpen();
    DownloadFileContentRanges(filePath, ranges, mtime, fileOpen,
                              false);  // sync
  }

  UploadFileCallback callback(filePath, node, m_cache, m_client,
                              m_directoryTree, this, releaseFile, updateMeta);
  if (async) {
    GetTransferManager()->GetExecutor()->SubmitAsync(
        bind(boost::type<void>(), callback, _1),
        bind(boost::type<shared_ptr<TransferHandle> >(),
             &QS::Client::TransferManager::UploadFile, m_transferManager.get(),
             _1, fileSize, mtime, m_cache, false),
        filePath);
  } else {
    callback(m_transferManager->UploadFile(filePath, fileSize, mtime, m_cache));
  }
}

// --------------------------------------------------------------------------
void Drive::ReleaseFile(const string &filePath) {
  shared_ptr<Node> node = GetNodeSimple(filePath);

  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return;
  }

  Info("Close file " + FormatPath(filePath));
  node->SetNeedUpload(false);
  node->SetFileOpen(false);
  if (m_cache->HasFile(filePath)) {
    m_cache->SetFileOpen(filePath, false);
  }
}

// --------------------------------------------------------------------------
void Drive::Utimens(const string &path, time_t mtime) {
  // TODO(jim): wait for sdk meta data api
  // x-qs-meta-mtime
  // x-qs-copy-source
  // x-qs-metadata-directive = REPLACE
  // call Stat to update meta locally
  // NOTE just do this with put object copy (this will delete orginal file
  // then create a copy of it)
}

// --------------------------------------------------------------------------
int Drive::WriteFile(const string &filePath, off_t offset, size_t size,
                     const char *buf) {
  shared_ptr<Node> node = GetNodeSimple(filePath);
  if (!(node && *node)) {
    Warning("File not exist " + FormatPath(filePath));
    return 0;
  }

  bool isOpen = node->IsFileOpen();
  time_t mtime = node->GetMTime();
  bool success = m_cache->Write(filePath, offset, size, buf, mtime, isOpen);
  if (success) {
    node->SetNeedUpload(true);
    if (offset + size > node->GetFileSize()) {
      node->SetFileSize(offset + size);
    }
  }

  return success ? size : 0;
}

// --------------------------------------------------------------------------
void Drive::DownloadFileContentRanges(const string &filePath,
                                      const ContentRangeDeque &ranges,
                                      time_t mtime, bool open, bool async) {
  BOOST_FOREACH (const ContentRangeDeque::value_type &range, ranges) {
    DownloadFileContentRange(filePath, range, mtime, open, async);
  }
}

// --------------------------------------------------------------------------
struct DownloadFileContentRangeCallback {
  string filePath;
  off_t offset;
  size_t downloadSize;
  shared_ptr<IOStream> stream;
  time_t mtime;
  bool fileOpen;
  shared_ptr<Cache> cache;

  DownloadFileContentRangeCallback(const string &filePath_, off_t offset_,
                                   size_t downloadSize_,
                                   const shared_ptr<IOStream> &stream_,
                                   time_t mtime_, bool open_,
                                   const shared_ptr<Cache> &cache_)
      : filePath(filePath_),
        offset(offset_),
        downloadSize(downloadSize_),
        stream(stream_),
        mtime(mtime_),
        fileOpen(open_),
        cache(cache_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle) {
      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        bool success = cache->Write(filePath, offset, downloadSize, stream,
                                    mtime, fileOpen);
        DebugErrorIf(!success,
                     "Fail to write cache [file:offset:len=" + filePath + ":" +
                         to_string(offset) + ":" + to_string(downloadSize) +
                         "]");
      }
    }
  }
};

// --------------------------------------------------------------------------
void Drive::DownloadFileContentRange(const string &filePath,
                                     const pair<off_t, size_t> &range,
                                     time_t mtime, bool open, bool async) {
  off_t offset = range.first;
  size_t size = range.second;
  // Download file if not found in cache or if cache need update
  bool fileContentExist = m_cache->HasFileData(filePath, offset, size);
  if (!fileContentExist) {
    uint64_t bufSize = QS::Client::ClientConfiguration::Instance()
                           .GetTransferBufferSizeInMB() *
                       QS::Size::MB1;
    size_t remainingSize = size;
    uint64_t downloadedSize = 0;

    while (remainingSize > 0) {
      off_t offset_ = offset + downloadedSize;
      int64_t downloadSize_ = remainingSize > bufSize ? bufSize : remainingSize;
      if (downloadSize_ <= 0) {
        break;
      }

      shared_ptr<IOStream> stream_ = make_shared<IOStream>(downloadSize_);
      DownloadFileContentRangeCallback callback(
          filePath, offset_, downloadSize_, stream_, mtime, open, m_cache);

      if (async) {
        GetTransferManager()->GetExecutor()->SubmitAsync(
            bind(boost::type<void>(), callback, _1),
            bind(boost::type<shared_ptr<TransferHandle> >(),
                 &QS::Client::TransferManager::DownloadFile,
                 m_transferManager.get(), _1, offset_, downloadSize_, stream_,
                 false),
            filePath);
      } else {
        shared_ptr<TransferHandle> handle = m_transferManager->DownloadFile(
            filePath, offset_, downloadSize_, stream_);
        callback(handle);
      }

      downloadedSize += downloadSize_;
      remainingSize -= downloadSize_;
    }
  }
}

}  // namespace FileSystem
}  // namespace QS
