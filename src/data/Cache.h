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

namespace QS {

namespace Client {
class QSClient;
}  // namespace Client

namespace FileSystem {
class Drive;
struct RenameDirCallback;
struct RemoveFileCallback;
}  // namespace FileSystem

namespace Data {

class DirectoryTree;
class File;

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

  // Whether a file exists in cache
  //
  // @param  : file path
  // @return : bool
  bool HasFile(const std::string &filePath) const;

  // Return the number of files in cache
  size_t GetNumFile() const;

  // Get cache size
  uint64_t GetSize() const;

  // Get cache Capacity
  uint64_t GetCapacity() const { return m_capacity; }

  // Find the file
  //
  // @param  : file path (absolute path)
  // @return : shared_ptr<File>
  boost::shared_ptr<File> FindFile(const std::string &filePath);

  // Begin of cache list
  CacheListIterator Begin();

  // End of cache list
  CacheListIterator End();

  // Make a new file
  boost::shared_ptr<File> MakeFile(const std::string &fileId);

 private:

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

  //  Move the file into the front of the cache
  void MakeFileMostRecentlyUsed(const std::string &fileId);

 private:
  // Add size
  void AddSize(uint64_t delta);

  // Subtract size
  void SubtractSize(uint64_t delta);

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

  friend class QS::Data::File;
  friend class QS::Client::QSClient;
  friend class QS::FileSystem::Drive;
  friend struct QS::FileSystem::RenameDirCallback;
  friend struct QS::FileSystem::RemoveFileCallback;
  friend class CacheTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_CACHE_H_
