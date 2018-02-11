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

#ifndef QSFS_DATA_FILEMETADATA_H_
#define QSFS_DATA_FILEMETADATA_H_

#include <stdint.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "boost/shared_ptr.hpp"

namespace QS {

namespace Data {

class Entry;
class FileMetaData;
class FileMetaDataManager;

struct FileType {
  enum Value { File, Directory, SymLink, Block, Character, FIFO, Socket };
};

std::string GetFileTypeName(FileType::Value fileType);
// Build a default dir meta
// Set mtime = 0 by default, so as any update based on the condition that if dir
// is modified should still be available.
boost::shared_ptr<QS::Data::FileMetaData> BuildDefaultDirectoryMeta(
    const std::string &dirPath, time_t mtime = 0);

/**
 * Object file metadata
 */
class FileMetaData {
 public:
  FileMetaData(const std::string &filePath, uint64_t fileSize, time_t atime,
               time_t mtime, uid_t uid, gid_t gid, mode_t fileMode,
               FileType::Value fileType = FileType::File,
               const std::string &mimeType = "",
               const std::string &eTag = std::string(), bool encrypted = false,
               dev_t dev = 0, int numlink = 1);

  bool operator==(const FileMetaData &rhs) const;

 public:
  struct stat ToStat() const;
  mode_t GetFileTypeAndMode() const;
  bool IsDirectory() const { return m_fileType == FileType::Directory; }
  // Return the directory path (ending with "/") this file belongs to
  std::string MyDirName() const;
  std::string MyBaseName() const;
  bool FileAccess(uid_t uid, gid_t gid, int amode) const;

  // accessor
  const std::string &GetFilePath() const { return m_filePath; }
  time_t GetMTime() const { return m_mtime; }
  bool IsFileOpen() const { return m_fileOpen; }
  bool IsNeedUpload() const {return m_needUpload;}

 private:
  FileMetaData() {}

  // file full path name
  std::string m_filePath;  // For a directory, this will be ending with "/"
  uint64_t m_fileSize;
  // Notice: file creation time is not stored in unix
  time_t m_atime;  // time of last access
  time_t m_mtime;  // time of last modification
  time_t m_ctime;  // time of last file status change
  time_t m_cachedTime;
  uid_t m_uid;        // user ID of owner
  gid_t m_gid;        // group ID of owner
  mode_t m_fileMode;  // file type & mode (permissions)
  FileType::Value m_fileType;
  std::string m_mimeType;
  std::string m_eTag;
  bool m_encrypted;
  dev_t m_dev;  // device number (file system)
  int m_numLink;
  bool m_needUpload;
  bool m_fileOpen;

  friend class Entry;
  friend class FileMetaDataManager;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_FILEMETADATA_H_
