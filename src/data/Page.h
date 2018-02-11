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

#ifndef QSFS_DATA_PAGE_H_
#define QSFS_DATA_PAGE_H_

#include <stddef.h>  // for size_t

#include <sys/types.h>  // for off_t

#include <iostream>
#include <set>
#include <string>

#include "boost/shared_ptr.hpp"
#include "boost/thread/recursive_mutex.hpp"

namespace QS {

namespace Data {

class File;

class Page {
 private:
  // Page attributes
  //
  // +-----------------------------------------+
  // | A File composed of two successive pages |
  // +-----------------------------------------+
  //
  // offset  stop  next   <= 1st page
  //   ^        ^  ^
  //   |________|__|________
  //   |<- size  ->|        |
  //   |___________|________|
  //   0  1  2  3  4  5  6  7
  //               ^     ^  ^
  //          offset  stop  next   <= 2nd page
  //
  // 1st Page: offset = 0, size = 4, stop = 3, next = 4
  // 2nd Page: offset = 4, size = 3, stop = 6, next = 7

  off_t m_offset;  // offset from the begin of owning File
  size_t m_size;   // size of bytes this page contains

  // NOTICE: body stream should be QS::Data::IOStream which associated with
  // QS::Data::StreamBuf when not use disk file; otherwise body stream is a
  // fstream assoicated to a disk file.
  // With the asssoicated stream buf or disk file, the body stream support to
  // be read/write for multiple times, but keep in mind following things:
  // 1) always seek to the right postion before to read/write body stream;
  // 2) if use disk file, always use RAII FileOpener to open it for read/write
  boost::shared_ptr<std::iostream> m_body;  // stream storing the bytes

  std::string m_diskFile;  // disk file is used when in-memory cache is not
                           // available, it is an absolute file path

  mutable boost::recursive_mutex m_mutex;

 private:
  Page() : m_offset(0), m_size(0) {}

 public:
  // Construct Page from a block of bytes
  //
  // @param  : file offset, len of bytes, buffer
  // @return :
  //
  // From pointer of buffer, number of len bytes will be writen.
  // The owning file's offset is 'offset'.
  Page(off_t offset, size_t len, const char *buffer);

  // Construct Page from a block of bytes (store it in disk file)
  //
  // @param  : file offset, len, buffer, disk file path
  // @return :
  Page(off_t offset, size_t len, const char *buffer,
       const std::string &diskfile);

  // Construct Page from a stream
  //
  // @param  : file offset, len of bytes, stream
  // @return :
  //
  // From stream, number of len bytes will be writen.
  // The owning file's offset is 'offset'.
  Page(off_t offset, size_t len,
       const boost::shared_ptr<std::iostream> &stream);

  // Construct Page from a stream (store it in disk file)
  //
  // @param  : file offset, len of bytes, stream, disk file
  // @return :
  Page(off_t offset, size_t len, const boost::shared_ptr<std::iostream> &stream,
       const std::string &diskfile);

 public:
  ~Page() {}

  // Return the stop position.
  off_t Stop() const { return 0 < m_size ? m_offset + m_size - 1 : 0; }

  // Return the offset of the next successive page.
  off_t Next() const { return m_offset + m_size; }

  // Return the size
  size_t Size() const { return m_size; }

  // Return the offset
  off_t Offset() const { return m_offset; }

  // Return body
  const boost::shared_ptr<std::iostream> &GetBody() const { return m_body; }

  // Return if page use disk file
  bool UseDiskFile();
  bool UseDiskFileNoLock();

  // Refresh the page's partial content
  //
  // @param  : file offset, len of bytes to update, buffer, disk file
  // @return : bool
  //
  // May enlarge the page's size depended on 'len', and when the len
  // is larger than page's size and using disk file, then all page's data
  // will be put to disk file.
  bool Refresh(off_t offset, size_t len, const char *buffer,
               const std::string &diskfile = std::string());

  // Refresh the page's entire content with bytes from buffer,
  // without checking.
  // For internal use only.
  // Refresh the page's entire content
  //
  // @param  : buffer
  // @return : bool
  bool Refresh(const char *buffer) { return Refresh(m_offset, m_size, buffer); }

  // Read the page's content
  //
  // @param  : file offset, len of bytes to read, buffer
  // @return : size of readed bytes
  size_t Read(off_t offset, size_t len, char *buffer);

  // Read the page's partial content
  // Starting from file offset, all the page's remaining size will be read.
  size_t Read(off_t offset, char *buffer) {
    return Read(offset, Next() - offset, buffer);
  }

  // Read the page's partial content
  size_t Read(size_t len, char *buffer) { return Read(m_offset, len, buffer); }

  // Read the page's entire content to buffer.
  size_t Read(char *buffer) { return Read(m_offset, m_size, buffer); }

 private:
  // Set stream
  void SetStream(const boost::shared_ptr<std::iostream> &stream);

  // Setup disk file on disk
  // - open the disk file
  // - set stream to fstream assocating with disk file
  bool SetupDiskFile();

  // Do a lazy resize for page.
  void ResizeToSmallerSize(size_t smallerSize);

  // Put data to body
  // For internal use only
  void UnguardedPutToBody(off_t offset, size_t len, const char *buffer);
  void UnguardedPutToBody(off_t offset, size_t len,
                          const boost::shared_ptr<std::iostream> &stream);

  // Refreseh the page's partial content without checking.
  // Starting from file offset, len of bytes will be updated.
  // For internal use only.
  bool UnguardedRefresh(off_t offset, size_t len, const char *buffer,
                        const std::string &diskfile = std::string());

  // Refresh the page's partial content without checking.
  // Starting from file offset, all the page's remaining size will be updated.
  // For internal use only.
  bool UnguardedRefresh(off_t offset, const char *buffer) {
    return UnguardedRefresh(offset, Next() - offset, buffer);
  }

  // Refresh the page's entire content with bytes from buffer,
  // without checking.
  // For internal use only.
  bool UnguardedRefresh(const char *buffer) {
    return UnguardedRefresh(m_offset, m_size, buffer);
  }

  // Read the page's partial content without checking
  // Starting from file offset, len of bytes will be read.
  // For internal use only.
  size_t UnguardedRead(off_t offset, size_t len, char *buffer);

  // Read the page's partial content without checking
  // Starting from file offset, all the page's remaining size will be read.
  // For internal use only.
  size_t UnguardedRead(off_t offset, char *buffer) {
    return UnguardedRead(offset, Next() - offset, buffer);
  }

  // Read the page's partial content without checking
  // For internal use only.
  size_t UnguardedRead(size_t len, char *buffer) {
    return UnguardedRead(m_offset, len, buffer);
  }

  // Read the page's entire content to buffer.
  // For internal use only.
  size_t UnguardedRead(char *buffer) {
    return UnguardedRead(m_offset, m_size, buffer);
  }

  friend class File;
  friend class PageTest;
};

struct PageCmp {
  bool operator()(const boost::shared_ptr<Page> &a,
                  const boost::shared_ptr<Page> &b) const {
    return a->Offset() < b->Offset();
  }
};

typedef std::set<boost::shared_ptr<Page>, PageCmp> PageSet;
typedef PageSet::const_iterator PageSetConstIterator;

std::string ToStringLine(const std::string &fileId, off_t offset, size_t len,
                         const char *buffer);
std::string ToStringLine(off_t offset, size_t len, const char *buffer);
std::string ToStringLine(off_t offset, size_t size);

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_PAGE_H_
