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

#ifndef QSFS_DATA_FILE_H_
#define QSFS_DATA_FILE_H_

#include <stddef.h>  // for size_t
#include <time.h>

#include <deque>
#include <iostream>
#include <list>
#include <string>
#include <utility>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/tuple/tuple.hpp"

#include "data/Page.h"

namespace QS {

namespace Client {
class Client;
class TransferManager;
class QSTransferManager;
}

namespace FileSystem {
class Drive;
}

namespace Data {
class Cache;
class DirectoryTree;
struct DownloadRangeCallback;

// Range represented by a pair of {offset, size}
typedef std::deque<std::pair<off_t, size_t> > ContentRangeDeque;

class File : private boost::noncopyable {
 public:
  explicit File(const std::string &filePath, size_t size = 0);

  ~File();

 public:
  std::string GetFilePath() const { 
    boost::lock_guard<boost::mutex> locker(m_filePathLock);
    return m_filePath;
  }
  std::string GetBaseName() const { return m_baseName; }

  size_t GetSize() const {
    boost::lock_guard<boost::mutex> locker(m_sizeLock);
    return m_size;
  }
  size_t GetCachedSize() const {
    boost::lock_guard<boost::mutex> locker(m_cacheSizeLock);
    return m_cacheSize;
  }
  bool UseDiskFile() const {
    boost::lock_guard<boost::mutex> locker(m_useDiskFileLock);
    return m_useDiskFile;
  }
  bool IsOpen() const {
    boost::lock_guard<boost::mutex> locker(m_openLock);
    return m_open;
  }

  // return disk file path
  std::string AskDiskFilePath() const;

  // Return a pair of iterators pointing to the range of consecutive pages
  // at the front of cache list
  //
  // @param  : void
  // @return : pair
  // Notice: this return a half-closed half open range [page1, page2)
  std::pair<PageSetConstIterator, PageSetConstIterator>
  ConsecutivePageRangeAtFront() const;

  // Whether the file containing the content
  //
  // @param  : content range start, content range size
  // @return : bool
  bool HasData(off_t start, size_t size) const;

  // Return the unexisting content ranges
  //
  // @param  : content range start, content range size
  // @return : a list of pair {range start, range size}
  ContentRangeDeque GetUnloadedRanges(off_t start, size_t size) const;

  // Return begin pos of pages
  PageSetConstIterator BeginPage() const;

  // Return end pos of pages
  PageSetConstIterator EndPage() const;

  // Return num of pages
  size_t GetNumPages() const;

  // To string
  std::string ToString() const;

  // Clear
  void Clear();

 private:
  // Read from the cache (file pages)
  //
  // @param  : file offset, len of bytes, buf
  // @return : a pair of {readed size, unloaded ranges}
  //
  // If any bytes is not present, download it as a new page.
  // Pagelist of outcome is sorted by page offset.
  // Notes: buf at least has bytes of 'len' memory
  std::pair<size_t, ContentRangeDeque> Read(
      off_t offset, size_t len, char *buf,
      boost::shared_ptr<QS::Client::TransferManager> transferManager,
      boost::shared_ptr<QS::Data::DirectoryTree> dirTree,
      boost::shared_ptr<QS::Data::Cache> cache);

  // For internal use
  // Read from the cache with no load
  // Notes: buf at least has bytes of 'len' memory
  std::pair<size_t, ContentRangeDeque> ReadNoLoad(off_t offset, size_t len,
                                                  char *buf) const;

  // Write a block of bytes into pages
  //
  // @param  : file offset, len, buffer
  // @return : {success, added size in cache, added size}
  //
  // From pointer of buffer, number of len bytes will be writen.
  // The owning file's offset is set with 'offset'.
  boost::tuple<bool, size_t, size_t> Write(
      off_t offset, size_t len, const char *buffer,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);

  // Write stream into pages
  //
  // @param  : file offset, len of stream, stream
  // @return : {success, added size in cache, added size}
  //
  // The stream will be moved to the pages.
  // The owning file's offset is set with 'offset'.
  boost::tuple<bool, size_t, size_t> Write(
      off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream,
      const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
      const boost::shared_ptr<QS::Data::Cache> &cache);

  // Setup pre to write
  // For internal use
  bool PreWrite(size_t len, const boost::shared_ptr<QS::Data::Cache> &cache);

  // Setup post to write
  // For internal use
  void PostWrite(off_t offset, size_t len, size_t addedCacheSize,
                 const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
                 const boost::shared_ptr<QS::Data::Cache> &cache);

  // For internal use
  boost::tuple<bool, size_t, size_t> DoWrite(off_t offset, size_t len,
                                             const char *buffer);

  // For internal use
  boost::tuple<bool, size_t, size_t> DoWrite(
      off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream);

  void Flush(size_t fileSize,
             boost::shared_ptr<QS::Client::TransferManager> transferManager,
             boost::shared_ptr<QS::Data::DirectoryTree> dirTree,
             boost::shared_ptr<QS::Data::Cache> cache,
             boost::shared_ptr<QS::Client::Client> client,
             bool releaseFile, bool updateMeta, bool async = false);
  void Load(off_t offset, size_t size,
            boost::shared_ptr<QS::Client::TransferManager> transferManager,
            boost::shared_ptr<QS::Data::DirectoryTree> dirTree,
            boost::shared_ptr<QS::Data::Cache> cache, bool async = false);

  // Resize
  void Resize(size_t newSize,
              const boost::shared_ptr<QS::Data::DirectoryTree> &dirTree,
              const boost::shared_ptr<QS::Data::Cache> &cache);

  // Remove disk file
  void RemoveDiskFileIfExists(bool logOn = true) const;

  // Set flag to use disk file
  void SetUseDiskFile(bool useDiskFile) {
    boost::lock_guard<boost::mutex> locker(m_useDiskFileLock);
    m_useDiskFile = useDiskFile;
  }

  // Set file open state
  void SetOpen(bool open) {
    boost::lock_guard<boost::mutex> locker(m_openLock);
    m_open = open;
  }

  void SetOpen(bool open, boost::shared_ptr<QS::Data::DirectoryTree> dirTree);

  // Set size
  void SetSize(size_t sz) {
    boost::lock_guard<boost::mutex> locker(m_sizeLock);
    m_size = sz;
  }
  // Set cached size
  void SetCachedSize(size_t sz) {
    boost::lock_guard<boost::mutex> locker(m_cacheSizeLock);
    m_cacheSize = sz;
  }
  // Add size
  void AddSize(size_t delta) {
    boost::lock_guard<boost::mutex> locker(m_sizeLock);
    m_size += delta;
  }
  // Add cached size
  void AddCachedSize(size_t delta) {
    boost::lock_guard<boost::mutex> locker(m_cacheSizeLock);
    m_cacheSize += delta;
  }

  // Subtract size
  void SubtractSize(size_t delta) {
    boost::lock_guard<boost::mutex> locker(m_sizeLock);
    m_size -= delta;
  }
  // Substract cached size
  void SubtractCachedSize(size_t delta) {
    boost::lock_guard<boost::mutex> locker(m_cacheSizeLock);
    m_cacheSize -= delta;
  }

  // Rename
  void Rename(const std::string &newFilePath);

  // Returns an iterator pointing to the first Page that is not ahead of offset.
  // If no such Page is found, a past-the-end iterator is returned.
  PageSetConstIterator LowerBoundPage(off_t offset) const;
  // internal use only
  PageSetConstIterator LowerBoundPageNoLock(off_t offset) const;

  // Returns an iterator pointing to the first Page that is behind of offset.
  // If no such Page is found, a past-the-end iterator is returned.
  PageSetConstIterator UpperBoundPage(off_t offset) const;
  // internal use only
  PageSetConstIterator UpperBoundPageNoLock(off_t offset) const;

  // Returns a pair iterators pointing the pages which intesecting with
  // the range (from off1 to off2).
  // The first member pointing to first page not ahead of (could be
  // intersecting with) off1; the second member pointing to the first
  // page not ahead of off2, same as LowerBoundPage(off2).
  // Notice: this is a half-closed half open range [page1, page2)
  std::pair<PageSetConstIterator, PageSetConstIterator> IntesectingRange(
      off_t off1, off_t off2) const;

  // Return the first key in the page set.
  const boost::shared_ptr<Page> &Front();

  // Return the last key in the page set.
  const boost::shared_ptr<Page> &Back();

  void DownloadRanges(
      const ContentRangeDeque &ranges,
      boost::shared_ptr<QS::Client::TransferManager> transferManager,
      boost::shared_ptr<QS::Data::DirectoryTree> dirTree,
      boost::shared_ptr<QS::Data::Cache> cache, bool async = false);
  void DownloadRange(
      off_t offset, size_t len,
      boost::shared_ptr<QS::Client::TransferManager> transferManager,
      boost::shared_ptr<QS::Data::DirectoryTree> dirTree,
      boost::shared_ptr<QS::Data::Cache> cache, bool async = false);

  // Add a new page from a block of character without checking input.
  // Return {pointer to addedpage, success, added size in cache, added size}
  // internal use only
  boost::tuple<PageSetConstIterator, bool, size_t, size_t> UnguardedAddPage(
      off_t offset, size_t len, const char *buffer);
  boost::tuple<PageSetConstIterator, bool, size_t, size_t> UnguardedAddPage(
      off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream);

 private:
  mutable boost::mutex m_filePathLock;
  std::string m_filePath;
  std::string m_baseName;  // file base name

  mutable boost::mutex m_sizeLock;
  size_t m_size;   // record sum of all pages' size
                   // this will not include unload data size

  mutable boost::mutex m_cacheSizeLock;
  size_t m_cacheSize;  // record sum of all pages' size
                       // stored in cache not including disk file

  mutable boost::mutex m_useDiskFileLock;
  bool m_useDiskFile;  // use disk file when no free cache space

  mutable boost::mutex m_openLock;
  bool m_open;         // file open/close state

  boost::mutex m_clearLock;  // clear file

  mutable boost::recursive_mutex m_mutex;
  PageSet m_pages;              // a set of pages suppose to be successive

  friend class Cache;  // TODO(jim): remove to avoid loop dependency
  friend class FileTest;
  friend class QS::Data::DownloadRangeCallback;
  friend class QS::FileSystem::Drive;
  friend class QS::Client::QSTransferManager;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_FILE_H_
