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

#ifndef QSFS_DATA_ENTRY_H_
#define QSFS_DATA_ENTRY_H_

#include <stdint.h>
#include <time.h>

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "boost/shared_ptr.hpp"
#include "boost/weak_ptr.hpp"

#include "data/FileMetaData.h"

namespace QS {

namespace Data {

class DirectoryTree;
class File;
class Node;

/**
 * Representation of an Entry of a Node in the directory tree.
 */
class Entry {
 private:
  Entry(const std::string &filePath, uint64_t fileSize, time_t atime,
        time_t mtime, uid_t uid, gid_t gid, mode_t fileMode,
        FileType::Value fileType = FileType::File,
        const std::string &mimeType = "",
        const std::string &eTag = std::string(), bool encrypted = false,
        dev_t dev = 0, int numlink = 1);

  explicit Entry(const boost::shared_ptr<FileMetaData> &fileMetaData);

 public:
  ~Entry() {}

  // You always need to check if the entry is operable before
  // invoke its member functions
  operator bool() const {
    boost::shared_ptr<FileMetaData> meta = m_metaData.lock();
    return meta ? !meta->m_filePath.empty() : false;
  }

  bool IsDirectory() const {
    return m_metaData.lock()->m_fileType == FileType::Directory;
  }
  bool IsSymLink() const {
    return m_metaData.lock()->m_fileType == FileType::SymLink;
  }

  // accessor
  const boost::weak_ptr<FileMetaData> &GetMetaData() const {
    return m_metaData;
  }
  const std::string &GetFilePath() const {
    return m_metaData.lock()->m_filePath;
  }
  uint64_t GetFileSize() const { return m_metaData.lock()->m_fileSize; }
  int GetNumLink() const { return m_metaData.lock()->m_numLink; }
  FileType::Value GetFileType() const { return m_metaData.lock()->m_fileType; }
  mode_t GetFileMode() const { return m_metaData.lock()->m_fileMode; }
  time_t GetMTime() const { return m_metaData.lock()->m_mtime; }
  time_t GetCachedTime() const { return m_metaData.lock()->m_cachedTime; }
  uid_t GetUID() const { return m_metaData.lock()->m_uid; }
  bool IsNeedUpload() const { return m_metaData.lock()->m_needUpload; }
  bool IsFileOpen() const { return m_metaData.lock()->m_fileOpen; }

  std::string MyDirName() const { return m_metaData.lock()->MyDirName(); }
  std::string MyBaseName() const { return m_metaData.lock()->MyBaseName(); }

  struct stat ToStat() const {
    return m_metaData.lock()->ToStat();
  }

  bool FileAccess(uid_t uid, gid_t gid, int amode) const {
    return m_metaData.lock()->FileAccess(uid, gid, amode);
  }

 private:
  void DecreaseNumLink() { --m_metaData.lock()->m_numLink; }
  void IncreaseNumLink() { ++m_metaData.lock()->m_numLink; }
  void SetFileSize(uint64_t size) { m_metaData.lock()->m_fileSize = size; }
  void SetNeedUpload(bool needUpload) {
    m_metaData.lock()->m_needUpload = needUpload;
  }
  void SetFileOpen(bool fileOpen) { m_metaData.lock()->m_fileOpen = fileOpen; }

  void Rename(const std::string &newFilePath);

 private:
  Entry() {}

  // Using weak_ptr as FileMetaDataManger will control file mete data life cycle
  boost::weak_ptr<FileMetaData> m_metaData;  // file meta data

  friend class DirectoryTree;
  friend class EntryTest;
  friend class File;  // for SetFileSize
  friend class Node;
  friend class NodeTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_ENTRY_H_
