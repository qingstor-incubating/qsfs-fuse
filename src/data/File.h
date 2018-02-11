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

namespace Data {
class Cache;

// Range represented by a pair of {offset, size}
typedef std::deque<std::pair<off_t, size_t> > ContentRangeDeque;

class File : private boost::noncopyable {
 public:
  explicit File(const std::string &baseName, time_t mtime, size_t size = 0)
      : m_baseName(baseName),
        m_mtime(mtime),
        m_size(size),
        m_cacheSize(size),
        m_useDiskFile(false),
        m_open(false) {}

  ~File();

 public:
  std::string GetBaseName() const { return m_baseName; }
  size_t GetSize() const {
    boost::lock_guard<boost::mutex> locker(m_sizeLock);
    return m_size;
  }
  size_t GetCachedSize() const {
    boost::lock_guard<boost::mutex> locker(m_cacheSizeLock);
    return m_cacheSize;
  }
  time_t GetTime() const {
    boost::lock_guard<boost::mutex> locker(m_mtimeLock);
    return m_mtime;
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

 private:
  // Read from the cache (file pages)
  //
  // @param  : file offset, len of bytes, modified time since from
  // @return : a pair of {read size, page list containing data, unloaded ranges}
  //
  // If the file mtime is newer than m_mtime, clear cache and download file.
  // If any bytes is not present, download it as a new page.
  // Pagelist of outcome is sorted by page offset.
  //
  // Notes: the pagelist of outcome could containing more bytes than given
  // input asking for, for example, the 1st page of outcome could has a
  // offset which is ahead of input 'offset'.
  boost::tuple<size_t, std::list<boost::shared_ptr<Page> >, ContentRangeDeque>
  Read(off_t offset, size_t len, time_t mtimeSince = 0);

  // Write a block of bytes into pages
  //
  // @param  : file offset, len, buffer, modification time
  // @return : {success, added size in cache, added size}
  //
  // From pointer of buffer, number of len bytes will be writen.
  // The owning file's offset is set with 'offset'.
  boost::tuple<bool, size_t, size_t> Write(off_t offset, size_t len,
                                           const char *buffer, time_t mtime,
                                           bool open = false);

  // Write stream into pages
  //
  // @param  : file offset, len of stream, stream, modification time
  // @return : {success, added size in cache, added size}
  //
  // The stream will be moved to the pages.
  // The owning file's offset is set with 'offset'.
  boost::tuple<bool, size_t, size_t> Write(
      off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream,
      time_t mtime, bool open = false);

  // Resize the total pages' size to a smaller size.
  void ResizeToSmallerSize(size_t smallerSize);

  // Remove disk file
  void RemoveDiskFileIfExists(bool logOn = true) const;

  // Clear pages and reset attributes.
  void Clear();

  // Set modification time
  void SetTime(time_t mtime) {
    boost::lock_guard<boost::mutex> locker(m_mtimeLock);
    m_mtime = mtime;
  }

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

  // Add a new page from a block of character without checking input.
  // Return {pointer to addedpage, success, added size in cache, added size}
  // internal use only
  boost::tuple<PageSetConstIterator, bool, size_t, size_t> UnguardedAddPage(
      off_t offset, size_t len, const char *buffer);
  boost::tuple<PageSetConstIterator, bool, size_t, size_t> UnguardedAddPage(
      off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream);

 private:
  std::string m_baseName;  // file base name

  mutable boost::mutex m_mtimeLock;
  time_t m_mtime;  // time of last modification

  mutable boost::mutex m_sizeLock;
  size_t m_size;   // record sum of all pages' size

  mutable boost::mutex m_cacheSizeLock;
  size_t m_cacheSize;  // record sum of all pages' size
                       // stored in cache not including disk file

  mutable boost::mutex m_useDiskFileLock;
  bool m_useDiskFile;  // use disk file when no free cache space

  mutable boost::mutex m_openLock;
  bool m_open;         // file open/close state

  mutable boost::recursive_mutex m_mutex;
  PageSet m_pages;              // a set of pages suppose to be successive

  friend class Cache;
  friend class FileTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_FILE_H_
