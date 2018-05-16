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

#ifndef QSFS_DATA_CACHE_H_
#define QSFS_DATA_CACHE_H_

#include <stddef.h>  // for size_t
#include <stdint.h>
#include <time.h>

#include <sys/types.h>  // for off_t

#include <iostream>
#include <list>
#include <string>
#include <utility>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/unordered_map.hpp"

#include "base/HashUtils.h"
#include "data/File.h"
#include "data/Page.h"

namespace QS {

namespace Client {
class QSClient;
}  // namespace Client

namespace FileSystem {
class Drive;
struct RenameDirCallback;
struct DownloadFileContentRangeCallback;
}  // namespace FileSystem

namespace Data {

class DirectoryTree;
struct DownloadRangeCallback;
struct FlushCallback;

typedef std::pair<std::string, boost::shared_ptr<File> > FileIdToFilePair;
typedef std::list<FileIdToFilePair> CacheList;
typedef CacheList::iterator CacheListIterator;
typedef CacheList::const_iterator CacheListConstIterator;
typedef boost::unordered_map<std::string, CacheListIterator,
                             HashUtils::StringHash>
    FileIdToCacheListIteratorMap;
typedef FileIdToCacheListIteratorMap::iterator CacheMapIterator;
typedef FileIdToCacheListIteratorMap::const_iterator CacheMapConstIterator;

//
// Cache (FileManager)
//
class Cache : private boost::noncopyable {
 public:
  explicit Cache(uint64_t capacity) : m_size(0), m_capacity(capacity) {}

  ~Cache() {}

 public:
  // Has available free space
  //
  // @param  : need size
  // @return : bool
  //
  // If cache files' size plus needSize surpass MaxCacheSize,
  // then there is no avaiable needSize space.
  bool HasFreeSpace(size_t needSize) const;  // size in byte

  // Is the last file in cache open
  //
  // @param  : void
  // @return : bool
  //
  // NOTES:
  // we put least recently used File at back and free the cache starting
  // from back, so IsLastFileOpen can be used as a condition when freeing cache.
  bool IsLastFileOpen() const;

  // Whether the file content existing
  //
  // @param  : file path, content range start, content range size
  // @return : bool
  bool HasFileData(const std::string &filePath, off_t start, size_t size) const;

  // Whether a file exists in cache
  //
  // @param  : file path
  // @return : bool
  bool HasFile(const std::string &filePath) const;

  // Return the unexisting content ranges for a given file
  //
  // @param  : file path, content range start, content range size
  // @return : a list of {range start, range size}
  ContentRangeDeque GetUnloadedRanges(const std::string &filePath, off_t start,
                                      size_t size) const;

  // Return the number of files in cache
  size_t GetNumFile() const;

  // Get cache size
  uint64_t GetSize() const { return m_size; }

  // Get cache Capacity
  uint64_t GetCapacity() const { return m_capacity; }

  // Get file size
  uint64_t GetFileSize(const std::string &filePath) const;

  // Return file string presentation
  std::string FileToString(const std::string &filePath) const;

  // Find the file
  //
  // @param  : file path (absolute path)
  // @return : shared_ptr<File>
  boost::shared_ptr<File> FindFile(const std::string &filePath);

  // Begin of cache list
  CacheListIterator Begin();

  // End of cache list
  CacheListIterator End();

  // Read file cache into a buffer
  //
  // @param  : file path, offset, len, buffer
  // @return : {size of bytes have been writen to buffer, unloaded ranges}
  //
  // If not found fileId in cache, create it in cache and load its pages.
  std::pair<size_t, ContentRangeDeque> Read(const std::string &fileId,
                                            off_t offset, size_t len,
                                            char *buffer);

  // Make a new file
  bool MakeFile(const std::string &fileId);

 private:
  // Write a block of bytes into file cache
  //
  // @param  : file path, file offset, len, buffer
  // @return : bool
  //
  // If File of fileId doesn't exist, create one.
  // From pointer of buffer, number of len bytes will be writen.
  bool Write(const std::string &fileId, off_t offset, size_t len,
             const char *buffer,
             const boost::shared_ptr<DirectoryTree> &dirTree,
             bool open = false);

  // Write stream into file cache
  //
  // @param  : file path, file offset, stream
  // @return : bool
  //
  // If File of fileId doesn't exist, create one.
  // Stream will be moved to cache.
  bool Write(const std::string &fileId, off_t offset, size_t len,
             const boost::shared_ptr<std::iostream> &stream,
             const boost::shared_ptr<DirectoryTree> &dirTree,
             bool open = false);

  // Prepare for Write
  //
  // @param  : file id, content data len
  // @return : {falg of success, pointer to File}
  //
  // internal use only
  std::pair<bool, boost::shared_ptr<File> > PrepareWrite(
      const std::string &fileId, size_t len);

  // Free cache space
  //
  // @param  : size need to be freed, file should not be freed
  // @return : bool
  //
  // Discard the least recently used File to make sure
  // there will be number of size avaiable cache space.
  bool Free(size_t size, const std::string &fileUnfreeable);  // size in byte

  // Remove disk files used to cache file content
  //
  // @param  : disk folder path, size need to be freed, file should not be freed
  // @return : bool
  //
  // Discard the least recently used File which cache data in disk file to make
  // sure there will be number of size avaiable disk free space in disk foler.
  bool FreeDiskCacheFiles(const std::string &diskfolder, size_t size,
                          const std::string &fileUnfreeable);

  // Remove file from cache
  //
  // @param  : file id
  // @return : iterator pointing to next file in cache list if remove
  // sucessfully, otherwise return past-the-end iterator.
  CacheListIterator Erase(const std::string &fildId);

  // Rename a file
  //
  // @param  : file id, new file id
  // @return : void
  void Rename(const std::string &oldFileId, const std::string &newFileId);

  // Change file open state
  //
  // @param  : file id, open state
  // @return : void
  void SetFileOpen(const std::string &fileId, bool open,
                   const boost::shared_ptr<DirectoryTree> &dirTree);

  // Resize a file
  //
  // @param  : file id, new file size
  // @return : void
  void Resize(const std::string &fileId,
              size_t newSize, const boost::shared_ptr<DirectoryTree> &dirTree);

 private:
  // Create an empty File with fileId in cache, without checking input.
  // If success return reference to insert file, else return m_cache.end().
  CacheListIterator UnguardedNewEmptyFile(const std::string &fileId);

  // Erase the file denoted by pos, without checking input.
  CacheListIterator UnguardedErase(FileIdToCacheListIteratorMap::iterator pos);

// Move the file denoted by pos into the front of the cache,
  // without checking input.
  CacheListIterator UnguardedMakeFileMostRecentlyUsed(CacheListIterator pos);

 private:
  // Record sum of the cache files' size, not including disk file
  uint64_t m_size;

  uint64_t m_capacity;  // in bytes

  mutable boost::recursive_mutex m_mutex;
  // Most recently used File is put at front,
  // Least recently used File is put at back.
  CacheList m_cache;

  FileIdToCacheListIteratorMap m_map;

  friend class QS::Client::QSClient;
  friend struct QS::Data::DownloadRangeCallback;
  friend struct QS::Data::FlushCallback;
  friend class QS::FileSystem::Drive;
  friend struct QS::FileSystem::RenameDirCallback;
  friend struct QS::FileSystem::DownloadFileContentRangeCallback;
  friend class CacheTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_CACHE_H_
