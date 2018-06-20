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

#include "data/File.h"

#include <assert.h>

#include <iterator>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "boost/bind.hpp"
#include "boost/exception/to_string.hpp"
#include "boost/make_shared.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/tuple/tuple.hpp"

#include "base/LogMacros.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/ThreadPool.h"
#include "base/Utils.h"
#include "base/UtilsWithLog.h"
#include "client/Client.h"
#include "client/ClientConfiguration.h"
#include "client/QSError.h"
#include "client/TransferHandle.h"
#include "client/TransferManager.h"
#include "configure/Options.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/IOStream.h"
#include "data/Node.h"
#include "data/StreamUtils.h"
#include "filesystem/Drive.h"

namespace QS {

namespace Data {

using boost::lock_guard;
using boost::make_shared;
using boost::make_tuple;
using boost::mutex;
using boost::recursive_mutex;
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::to_string;
using boost::tuple;
using QS::Client::Client;
using QS::Client::ClientError;
using QS::Client::TransferHandle;
using QS::Client::TransferManager;
using QS::Data::Cache;
using QS::Data::DirectoryTree;
using QS::Data::IOStream;
using QS::Data::Node;
using QS::Data::StreamUtils::GetStreamSize;
using QS::StringUtils::PointerAddress;
using QS::StringUtils::BoolToString;
using QS::StringUtils::ContentRangeDequeToString;
using QS::StringUtils::FormatPath;
using QS::UtilsWithLog::CreateDirectoryIfNotExists;
using QS::UtilsWithLog::IsSafeDiskSpace;
using std::iostream;
using std::list;
using std::make_pair;
using std::pair;
using std::reverse_iterator;
using std::string;
using std::vector;

namespace {

// Build a disk file absolute path
//
// @param  : file base name
// @return : string
//
string BuildDiskFilePath(const string &basename) {
  string qsfsDiskDir =
      QS::Configure::Options::Instance().GetDiskCacheDirectory();
  return qsfsDiskDir + basename;
}

// --------------------------------------------------------------------------
string PrintFileName(const string &file) { return "[file=" + file + "]"; }

}  // namespace

// --------------------------------------------------------------------------
File::File(const string &filePath, size_t size)
    : m_filePath(filePath),
      m_baseName(QS::Utils::GetBaseName(filePath)),
      m_dataSize(size),
      m_cacheSize(size),
      m_useDiskFile(false),
      m_open(false) {}

// --------------------------------------------------------------------------
File::~File() {
  // As pages using disk file will reference to the same disk file, so File
  // should manage the life cycle of the disk file.
  RemoveDiskFileIfExists(false);  // log off
}

// --------------------------------------------------------------------------
size_t File::GetSize() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (m_pages.empty()) {
    return 0;
  }
  reverse_iterator<PageSetConstIterator> riter = m_pages.rbegin();
  return static_cast<size_t>((*riter)->Next());
}

// --------------------------------------------------------------------------
string File::AskDiskFilePath() const { return BuildDiskFilePath(m_baseName); }

// --------------------------------------------------------------------------
pair<PageSetConstIterator, PageSetConstIterator>
File::ConsecutivePageRangeAtFront() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (m_pages.empty()) {
    return make_pair(m_pages.begin(), m_pages.begin());
  }
  PageSetConstIterator cur = m_pages.begin();
  PageSetConstIterator next = m_pages.begin();
  while (++next != m_pages.end()) {
    if ((*cur)->Next() < (*next)->Offset()) {
      break;
    }
    ++cur;
  }
  return make_pair(m_pages.begin(), next);
}

// --------------------------------------------------------------------------
bool File::HasData(off_t start, size_t size) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  off_t stop = static_cast<off_t>(start + size);
  pair<PageSetConstIterator, PageSetConstIterator> range =
      IntesectingRange(start, stop);
  if (range.first == range.second) {
    if (range.first == m_pages.end()) {
      if (size == 0 && start <= static_cast<off_t>(GetSize())) {
        return true;
      }
    }
    return false;
  }
  // find the consecutive pages at front [beg to cur]
  PageSetConstIterator beg = range.first;
  PageSetConstIterator cur = beg;
  PageSetConstIterator next = beg;
  while (++next != range.second) {
    if ((*cur)->Next() < (*next)->Offset()) {
      break;
    }
    ++cur;
  }

  return ((*beg)->Offset() <= start && stop <= (*cur)->Next());
}

// --------------------------------------------------------------------------
ContentRangeDeque File::GetUnloadedRanges(off_t start, size_t size) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  ContentRangeDeque ranges;
  if (size == 0) {
    return ranges;
  }

  if (GetSize() == 0 || m_pages.empty()) {
    ranges.push_back(make_pair(start, size));
    return ranges;
  }

  off_t stop = static_cast<off_t>(start + size);
  pair<PageSetConstIterator, PageSetConstIterator> range =
      IntesectingRange(start, stop);

  if (range.first == range.second) {
    ranges.push_back(make_pair(start, size));
    return ranges;
  }

  PageSetConstIterator cur = range.first;
  PageSetConstIterator next = range.first;
  // the beginning
  if (start < (*cur)->Offset()) {
    off_t off = start;
    size_t sz = static_cast<size_t>((*cur)->Offset() - off);
    ranges.push_back(make_pair(off, sz));
  }
  // the middle
  while (++next != range.second) {
    if ((*cur)->Next() < (*next)->Offset()) {
      if ((*next)->Offset() > stop) {
        break;
      }
      off_t off = (*cur)->Next();
      size_t size = static_cast<size_t>((*next)->Offset() - off);
      ranges.push_back(make_pair(off, size));
    }
    ++cur;
  }
  // the ending
  if ((*cur)->Next() < stop) {
    off_t off = (*cur)->Next();
    size_t size = static_cast<size_t>(stop - off);
    ranges.push_back(make_pair(off, size));
  }

  return ranges;
}

// --------------------------------------------------------------------------
PageSetConstIterator File::BeginPage() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_pages.begin();
}

// --------------------------------------------------------------------------
PageSetConstIterator File::EndPage() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_pages.end();
}

// --------------------------------------------------------------------------
size_t File::GetNumPages() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_pages.size();
}

// --------------------------------------------------------------------------
string File::ToString() const {
  return "[file:" + m_baseName + ", size:" + to_string(GetSize()) +
         ", datasize:" + to_string(GetDataSize()) +
         ", cachedsize:" + to_string(GetCachedSize()) +
         ", useDisk:" + BoolToString(m_useDiskFile) +
         ", open:" + BoolToString(m_open) +
         ", pages:" + PageSetToString(m_pages) + "]";
}

// --------------------------------------------------------------------------
pair<size_t, ContentRangeDeque> File::Read(
    off_t offset, size_t len, char *buf,
    shared_ptr<TransferManager> transferManager,
    shared_ptr<DirectoryTree> dirTree, shared_ptr<Cache> cache,
    shared_ptr<Client> client, bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[offset:" + to_string(offset) + ", len:" + to_string(len) + "] " +
            FormatPath(GetFilePath()));
  shared_ptr<Node> node = dirTree->Find(GetFilePath());
  if (!node) {
    Error("Not found node in directory tree " + FormatPath(GetFilePath()));
    ContentRangeDeque unloadedRanges;
    unloadedRanges.push_back(make_pair(offset, len));
    return make_pair(0, unloadedRanges);
  }

  // ajust size and calculate remaining size
  uint64_t fileSize = node->GetFileSize();
  uint64_t readSize = len;
  int64_t remainingSize = 0;
  if (offset + len > fileSize) {
    readSize = fileSize - offset;
    DebugInfo("Overflow [file size:" + to_string(fileSize) + "] " +
              " ajust read size to " + to_string(readSize) +
              FormatPath(GetFilePath()));
  } else {
    remainingSize = fileSize - (offset + len);
  }

  if (readSize == 0) {
    ContentRangeDeque unloadedRanges;
    return make_pair(0, unloadedRanges);
  }

  // load and read
  Load(offset, readSize, transferManager, dirTree, cache, client, false);
  pair<size_t, ContentRangeDeque> outcome = ReadNoLoad(offset, readSize, buf);

  // no prefetch
  // if (remainingSize > 0) {
  //   off_t off = offset + len;
  //   size_t transferBufSz =
  //       QS::Configure::Options::Instance().GetTransferBufferSizeInMB() *
  //       QS::Size::MB1;
  //   size_t len = remainingSize > transferBufSz ? transferBufSz : remainingSize;
  //   Load(off, len, transferManager, dirTree, cache, client, async);
  // }

  return outcome;
}

// --------------------------------------------------------------------------
pair<size_t, ContentRangeDeque> File::ReadNoLoad(off_t offset, size_t len,
                                                 char *buf) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[offset:" + to_string(offset) + ", len:" + to_string(len) + "] " +
            FormatPath(GetFilePath()));
  if (buf != NULL) {
    memset(buf, 0, len);
  }
  ContentRangeDeque unloadedRanges;
  size_t readSize = 0;
  bool isValidInput = offset >= 0 && len >= 0;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Invalid input " + ToStringLine(offset, len));
    return make_pair(readSize, unloadedRanges);
  }

  if (len == 0) {
    unloadedRanges.push_back(make_pair(offset, len));
    return make_pair(readSize, unloadedRanges);
  }

  // If pages is empty.
  if (m_pages.empty()) {
    unloadedRanges.push_back(make_pair(offset, len));
    return make_pair(readSize, unloadedRanges);
  }

  pair<PageSetConstIterator, PageSetConstIterator> range =
      IntesectingRange(offset, offset + len);
  PageSetConstIterator it1 = range.first;
  PageSetConstIterator it2 = range.second;
  off_t offset_ = offset;
  size_t len_ = len;
  // For pages which are not completely ahead of 'offset'
  // but ahead of 'offset + len'.
  while (it1 != it2) {
    if (len_ <= 0) break;
    const shared_ptr<Page> &page = *it1;
    if (offset_ < page->m_offset) {
      // Add unloaded page for bytes not present
      size_t lenNewPage = static_cast<size_t>(page->m_offset - offset_);
      unloadedRanges.push_back(make_pair(offset_, lenNewPage));
      offset_ = page->m_offset;
      len_ -= lenNewPage;
    } else {  // Collect existing pages.
      if (len_ <= static_cast<size_t>(page->Next() - offset_)) {
        if (buf != NULL) {
          page->Read(offset_, len_, buf + offset_ - offset);
        }
        readSize += len_;
        return make_pair(readSize, unloadedRanges);
      } else {
        if (buf != NULL) {
          page->Read(offset_, buf + offset_ - offset);
        }
        readSize += page->Next() - offset_;
        len_ -= page->Next() - offset_;
        offset_ = page->Next();
        ++it1;
      }
    }
  }  // end of while
  // Add unloaded range for bytes not present.
  if (len_ > 0) {
    unloadedRanges.push_back(make_pair(offset_, len_));
  }

  return make_pair(readSize, unloadedRanges);
}

// --------------------------------------------------------------------------
tuple<bool, size_t, size_t> File::Write(
    off_t offset, size_t len, const char *buffer,
    const shared_ptr<DirectoryTree> &dirTree, const shared_ptr<Cache> &cache) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[offset:" + to_string(offset) + ", len:" + to_string(len) + "] " +
            FormatPath(GetFilePath()));
  if (PreWrite(len, cache)) {
    tuple<bool, size_t, size_t> res = DoWrite(offset, len, buffer);
    bool success = boost::get<0>(res);
    if (success) {
      PostWrite(offset, len, boost::get<1>(res), dirTree, cache);
    }
    return res;
  } else {
    return make_tuple(false, 0, 0);
  }
}

// --------------------------------------------------------------------------
tuple<bool, size_t, size_t> File::Write(
    off_t offset, size_t len, const shared_ptr<iostream> &stream,
    const shared_ptr<DirectoryTree> &dirTree, const shared_ptr<Cache> &cache) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[offset:" + to_string(offset) + ", len:" + to_string(len) + "] " +
            FormatPath(GetFilePath()));
  if (PreWrite(len, cache)) {
    tuple<bool, size_t, size_t> res = DoWrite(offset, len, stream);
    bool success = boost::get<0>(res);
    if (success) {
      PostWrite(offset, len, boost::get<1>(res), dirTree, cache);
    }
    return res;
  } else {
    return make_tuple(false, 0, 0);
  }
}

// --------------------------------------------------------------------------
bool File::PreWrite(size_t len, const shared_ptr<Cache> &cache) {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (!cache) {
    return false;
  }
  cache->MakeFileMostRecentlyUsed(GetFilePath());
  bool availableFreeSpace = true;
  if (!cache->HasFreeSpace(len)) {
    availableFreeSpace = cache->Free(len, GetFilePath());
    if (!availableFreeSpace) {
      string diskfolder =
          QS::Configure::Options::Instance().GetDiskCacheDirectory();
      if (!CreateDirectoryIfNotExists(diskfolder)) {
        Error("Unable to mkdir for cache" + FormatPath(diskfolder));
        return false;
      }
      if (!IsSafeDiskSpace(diskfolder, len)) {
        if (!cache->FreeDiskCacheFiles(diskfolder, len, GetFilePath())) {
          Error("No available free disk space (" + to_string(len) +
                "bytes) for folder " + FormatPath(diskfolder));
          return false;
        }
      }  // check safe disk space
    }
  }
  SetUseDiskFile(!availableFreeSpace);
  return true;
}

// --------------------------------------------------------------------------
void File::PostWrite(off_t offset, size_t len, size_t addedCacheSize,
                     const shared_ptr<DirectoryTree> &dirTree,
                     const shared_ptr<Cache> &cache) {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (cache) {
    cache->AddSize(addedCacheSize);
  }
  if (dirTree) {
    shared_ptr<Node> node = dirTree->Find(GetFilePath());
    uint64_t newSize = static_cast<uint64_t>(offset + len);
    if (node && newSize > node->GetFileSize()) {
      node->SetFileSize(newSize);
    }
  }
}

// --------------------------------------------------------------------------
tuple<bool, size_t, size_t> File::DoWrite(off_t offset, size_t len,
                                          const char *buffer) {
  lock_guard<recursive_mutex> lock(m_mutex);
  bool isValidInput = offset >= 0 && len >= 0 && buffer != NULL;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Fail to write file with invalid input " +
               ToStringLine(offset, len, buffer));
    return make_tuple(false, 0, 0);
  }
  if (len == 0) {
    return make_tuple(true, 0, 0);
  }

  size_t addedSizeInCache = 0;
  size_t addedSize = 0;
  // If pages is empty.
  if (m_pages.empty()) {
    tuple<PageSetConstIterator, bool, size_t, size_t> res =
        UnguardedAddPage(offset, len, buffer);
    if (boost::get<1>(res)) {
      addedSizeInCache += boost::get<2>(res);
      addedSize += boost::get<3>(res);
    }

    return make_tuple(boost::get<1>(res), addedSizeInCache, addedSize);
  }

  bool success = true;
  pair<PageSetConstIterator, PageSetConstIterator> range =
      IntesectingRange(offset, offset + len);
  PageSetConstIterator it1 = range.first;
  PageSetConstIterator it2 = range.second;
  off_t offset_ = offset;
  size_t start_ = 0;
  size_t len_ = len;
  // For pages which are not completely ahead of 'offset'
  // but ahead of 'offset + len'.
  while (it1 != it2) {
    if (len_ <= 0) break;
    const shared_ptr<Page> &page = *it1;
    if (offset_ < page->m_offset) {  // Insert new page for bytes not present.
      size_t lenNewPage = static_cast<size_t>(page->m_offset - offset_);
      tuple<PageSetConstIterator, bool, size_t, size_t> res =
          UnguardedAddPage(offset_, lenNewPage, buffer + start_);
      if (!boost::get<1>(res)) {
        success = false;
        return make_tuple(false, addedSizeInCache, addedSize);
      } else {
        addedSizeInCache += boost::get<2>(res);
        addedSize += boost::get<3>(res);
      }

      offset_ = page->m_offset;
      start_ += lenNewPage;
      len_ -= lenNewPage;
    } else {  // Refresh the overlapped page's content.
      if (len_ <= static_cast<size_t>(page->Next() - offset_)) {
        return make_tuple(page->Refresh(offset_, len_, buffer + start_),
                          addedSizeInCache, addedSize);
      } else {
        size_t refLen = page->Next() - offset_;
        bool refresh = page->Refresh(offset_, refLen, buffer + start_);
        if (!refresh) {
          success = false;
          return make_tuple(false, addedSizeInCache, addedSize);
        }

        offset_ += refLen;
        start_ += refLen;
        len_ -= refLen;
        ++it1;
      }
    }
  }  // end of while
  // Insert new page for bytes not present.
  if (len_ > 0) {
    tuple<PageSetConstIterator, bool, size_t, size_t> res =
        UnguardedAddPage(offset_, len_, buffer + start_);
    if (boost::get<1>(res)) {
      success = true;

      addedSizeInCache += boost::get<2>(res);
      addedSize += boost::get<3>(res);
    } else {
      success = false;
    }
  }

  return make_tuple(success, addedSizeInCache, addedSize);
}

// --------------------------------------------------------------------------
tuple<bool, size_t, size_t> File::DoWrite(off_t offset, size_t len,
                                          const shared_ptr<iostream> &stream) {
  lock_guard<recursive_mutex> lock(m_mutex);
  size_t streamsize = GetStreamSize(stream);
  assert(len <= streamsize);
  if (!(len <= streamsize)) {
    DebugError(
        "Invalid input, stream buffer size is less than input 'len' parameter. "
        "[file:" +
        GetFilePath() + ", len:" + to_string(len) + "]");
    return make_tuple(false, 0, 0);
  }

  scoped_ptr<vector<char> > buf(new vector<char>(len));
  stream->seekg(0, std::ios_base::beg);
  stream->read(&(*buf)[0], len);

  return DoWrite(offset, len, &(*buf)[0]);
}

// --------------------------------------------------------------------------
struct FlushCallback {
  string filePath;
  uint64_t fileSize;
  shared_ptr<TransferManager> transferManager;
  shared_ptr<DirectoryTree> dirTree;
  shared_ptr<Client> client;
  bool updateMeta;

  FlushCallback(const string &filePath_, uint64_t fileSize_,
                const shared_ptr<TransferManager> &transferManager_,
                const shared_ptr<DirectoryTree> &dirTree_,
                const shared_ptr<Client> &client_, bool updateMeta_)
      : filePath(filePath_),
        fileSize(fileSize_),
        transferManager(transferManager_),
        dirTree(dirTree_),
        client(client_),
        updateMeta(updateMeta_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle && client) {
      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        Info("Done Upload file [size:" + to_string(fileSize) + "] " +
             FormatPath(filePath));
        // update meta
        if (updateMeta) {
          if(dirTree) {
            dirTree->Grow(client->GetObjectMeta(handle->GetObjectKey()));
          }
        }
      } else {
        if (handle->IsMultipart()) {
          transferManager->m_unfinishedMultipartUploadHandles.emplace(
              handle->GetObjectKey(), handle);
        }
      }  // Done Transfer
    }
  }
};

// --------------------------------------------------------------------------
void File::Flush(size_t fileSize, shared_ptr<TransferManager> transferManager,
                 shared_ptr<DirectoryTree> dirTree, shared_ptr<Cache> cache,
                 shared_ptr<Client> client, bool releaseFile, bool updateMeta,
                 bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[filesize:" + to_string(fileSize) + "]" +
            FormatPath(GetFilePath()));
  if (!transferManager || !dirTree || !cache) {
    DebugWarning("Invalid input");
    return;
  }
  // download unloaded pages for file
  // this is need as user could open a file and edit a part of it,
  // but you need the completed file in order to upload it.
  Load(0, fileSize, transferManager, dirTree, cache, client, async);

  if (releaseFile) {
    SetOpen(false, dirTree);
  }
  FlushCallback callback(GetFilePath(), fileSize, transferManager, dirTree,
                         client, updateMeta);
  if (async) {
    transferManager->GetExecutor()->SubmitAsync(
        bind(boost::type<void>(), callback, _1),
        bind(boost::type<shared_ptr<TransferHandle> >(),
             &QS::Client::TransferManager::UploadFile, transferManager.get(),
             _1, fileSize, this, false),
        GetFilePath());
  } else {
    callback(transferManager->UploadFile(GetFilePath(), fileSize, this));
  }
}

// --------------------------------------------------------------------------
void File::Load(off_t offset, size_t size,
                shared_ptr<TransferManager> transferManager,
                shared_ptr<DirectoryTree> dirTree, shared_ptr<Cache> cache,
                shared_ptr<Client> client, bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo("[offset:" + to_string(offset) + ", len:" + to_string(size) + "] " +
            FormatPath(GetFilePath()));
  if (size == 0) {
    return;
  }

  // head real size
  uint64_t fileSz;
  shared_ptr<FileMetaData> fileMeta = client->GetObjectMeta(GetFilePath());
  if (fileMeta) {
    fileSz = fileMeta->m_fileSize;
  }
  if (offset > 0 && fileSz <= offset) {
    return;
  }

  // download unloaded ranges
  size_t sizeT = fileSz > offset + size ? size : fileSz - offset;
  ContentRangeDeque ranges = GetUnloadedRanges(offset, sizeT);
  if (!ranges.empty()) {
    DebugInfo("Download unloaded ranges:" + ContentRangeDequeToString(ranges));
    DownloadRanges(ranges, transferManager, dirTree, cache, async);
  }

  // if surpass real file size, fill it
  if (offset + size > fileSz) {
    ContentRangeDeque ranges =
        GetUnloadedRanges(fileSz, offset + size - fileSz);
    if (!ranges.empty()) {
      BOOST_FOREACH (const ContentRangeDeque::value_type &range, ranges) {
        off_t off = range.first;
        size_t len = range.second; 
        // fill hole
        vector<char> hole(len);  // value initialization with '\0'
        DebugInfo("Fill hole [offset:" + to_string(off) +
                  ", len:" + to_string(len) + "] " + FormatPath(GetFilePath()));
        Write(off, len, &hole[0], dirTree, cache);
      }
    }
  }
}

// --------------------------------------------------------------------------
void File::Truncate(size_t newSize,
                    const shared_ptr<TransferManager> &transferManager,
                    const shared_ptr<DirectoryTree> &dirTree,
                    const shared_ptr<Cache> &cache,
                    const shared_ptr<Client> &client) {
  lock_guard<recursive_mutex> lock(m_mutex);
  DebugInfo(to_string(newSize));
  size_t oldFileSize = GetSize();
  if (newSize == oldFileSize) {
    return;
  }
  if (newSize > oldFileSize) {
    // fill the hole
    size_t holeSize = newSize - oldFileSize;
    vector<char> hole(holeSize);  // value initialization with '\0'
    DebugInfo("Fill hole [offset:" + to_string(oldFileSize) + ", len:" +
              to_string(holeSize) + "] " + FormatPath(GetFilePath()));
    Write(oldFileSize, holeSize, &hole[0], dirTree, cache);
  } else {
    // resize to smaller size
    while (!m_pages.empty() && newSize < GetSize()) {
      PageSetConstIterator lastPage = --m_pages.end();
      size_t lastPageSize = (*lastPage)->Size();
      if (newSize <= (*lastPage)->Offset()) {
        if (!(*lastPage)->UseDiskFile()) {
          m_cacheSize -= lastPageSize;
        }
        m_dataSize -= lastPageSize;
        m_pages.erase(lastPage);
        if (cache) {
          cache->SubtractSize(lastPageSize);
        }
      } else {
        size_t delta = GetSize() - newSize;
        // Do a lazy remove for last page.
        (*lastPage)->ResizeToSmallerSize(lastPageSize - delta);
        if (!(*lastPage)->UseDiskFile()) {
          m_cacheSize -= delta;
        }
        m_dataSize -= delta;
        // Lazy remove, no size change to cache
        break;
      }
    }

    // If file has unloaded pages, add a dummy pages inorder to maintain file
    // size
    if (newSize > GetSize()) {
      const char *dummy = "";
      m_pages.insert(shared_ptr<Page>(new Page(newSize, 0, dummy)));
    }

    if (dirTree) {
      shared_ptr<Node> node = dirTree->Find(GetFilePath());
      if (node) {
        node->SetFileSize(GetSize());
      }
    }
  }

  Flush(newSize, transferManager, dirTree, cache, client, false, false, false);

  // check
  if (GetSize() != newSize) {
    DebugWarning("Resize from " + to_string(oldFileSize) + " to " +
                 to_string(newSize) + "bytes, got file size " +
                 to_string(GetSize()) + FormatPath(GetFilePath()));
  }
}

// --------------------------------------------------------------------------
void File::Rename(const std::string &newFilePath) {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_filePath = newFilePath;
  m_baseName = QS::Utils::GetBaseName(newFilePath);
}

// --------------------------------------------------------------------------
void File::RemoveDiskFileIfExists(bool logOn) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (UseDiskFile()) {
    string diskFile = AskDiskFilePath();
    if (logOn) {
      if (QS::UtilsWithLog::FileExists(diskFile))
        QS::UtilsWithLog::RemoveFileIfExists(diskFile);
    } else {
      if (QS::Utils::FileExists(diskFile))
        QS::Utils::RemoveFileIfExists(diskFile);
    }
  }
}

// --------------------------------------------------------------------------
void File::Clear() {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_pages.clear();
  m_dataSize = 0;
  m_cacheSize = 0;
  RemoveDiskFileIfExists(true);
  m_useDiskFile = false;
}

// --------------------------------------------------------------------------
void File::SetOpen(bool open, shared_ptr<DirectoryTree> dirTree) {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_open = open;
  if (dirTree) {
    shared_ptr<Node> node = dirTree->Find(GetFilePath());
    if (node) {
      node->SetFileOpen(open);
    }
  }
}

// --------------------------------------------------------------------------
PageSetConstIterator File::LowerBoundPage(off_t offset) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return LowerBoundPageNoLock(offset);
}

// --------------------------------------------------------------------------
PageSetConstIterator File::LowerBoundPageNoLock(off_t offset) const {
  shared_ptr<Page> tmpPage =
      make_shared<Page>(offset, 0, make_shared<IOStream>(0));
  return m_pages.lower_bound(tmpPage);
}

// --------------------------------------------------------------------------
PageSetConstIterator File::UpperBoundPage(off_t offset) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return UpperBoundPageNoLock(offset);
}

// --------------------------------------------------------------------------
PageSetConstIterator File::UpperBoundPageNoLock(off_t offset) const {
  shared_ptr<Page> tmpPage =
      make_shared<Page>(offset, 0, make_shared<IOStream>(0));
  return m_pages.upper_bound(tmpPage);
}

// --------------------------------------------------------------------------
pair<PageSetConstIterator, PageSetConstIterator> File::IntesectingRange(
    off_t off1, off_t off2) const {
  assert(off1 <= off2);
  lock_guard<recursive_mutex> lock(m_mutex);
  PageSetConstIterator it1 = LowerBoundPageNoLock(off1);
  PageSetConstIterator it2 = LowerBoundPageNoLock(off2);
  // Move backward it1 to pointing to the page which maybe intersect with
  // 'offset'.
  reverse_iterator<PageSetConstIterator> reverseIt(it1);
  it1 = (reverseIt == m_pages.rend() || (*reverseIt)->Next() <= off1)
            ? it1
            : (++reverseIt).base();

  return make_pair(it1, it2);
}

// --------------------------------------------------------------------------
const shared_ptr<Page> &File::Front() {
  lock_guard<recursive_mutex> lock(m_mutex);
  assert(!m_pages.empty());
  return *(m_pages.begin());
}

// --------------------------------------------------------------------------
const shared_ptr<Page> &File::Back() {
  lock_guard<recursive_mutex> lock(m_mutex);
  assert(!m_pages.empty());
  return *(m_pages.rbegin());
}

// --------------------------------------------------------------------------
struct DownloadRangeCallback {
  string filePath;
  off_t offset;
  size_t downloadSize;
  shared_ptr<IOStream> stream;
  shared_ptr<Cache> cache;
  shared_ptr<DirectoryTree> dirTree;
  File *file;

  DownloadRangeCallback(const string &filePath_, off_t offset_,
                        size_t downloadSize_,
                        const shared_ptr<IOStream> &stream_,
                        const shared_ptr<Cache> &cache_,
                        const shared_ptr<DirectoryTree> &dirTree_, File *file_)
      : filePath(filePath_),
        offset(offset_),
        downloadSize(downloadSize_),
        stream(stream_),
        cache(cache_),
        dirTree(dirTree_),
        file(file_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle) {
      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        if (file) {
          tuple<bool, size_t, size_t> res =
              file->Write(offset, downloadSize, stream, dirTree, cache);
          ErrorIf(!boost::get<0>(res),
                  "Fail to write cache [file:" + filePath +
                      ", offset:" + to_string(offset) +
                      ", len:" + to_string(downloadSize) + "]");
        }
      } else {
        string msg = "Fail to download [offset:" + to_string(offset) +
                     ", len:" + to_string(downloadSize) + "]";
        if (file) {
          msg += file->ToString();
        }
        msg += FormatPath(filePath);
        Error(msg);
      }
    }
  }
};

// --------------------------------------------------------------------------
void File::DownloadRanges(const ContentRangeDeque &ranges,
                          shared_ptr<TransferManager> transferManager,
                          shared_ptr<DirectoryTree> dirTree,
                          shared_ptr<Cache> cache, bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  BOOST_FOREACH (const ContentRangeDeque::value_type &range, ranges) {
    off_t offset = range.first;
    size_t len = range.second;
    DownloadRange(offset, len, transferManager, dirTree, cache, async);
  }
}

// --------------------------------------------------------------------------
void File::DownloadRange(off_t offset, size_t size,
                         shared_ptr<TransferManager> transferManager,
                         shared_ptr<DirectoryTree> dirTree,
                         shared_ptr<Cache> cache, bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  bool fileContentExist = HasData(offset, size);
  if (fileContentExist) {
    return;
  }
  if (!transferManager) {
    return;
  }
  uint64_t bufSize =
      QS::Client::ClientConfiguration::Instance().GetTransferBufferSizeInMB() *
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
    DownloadRangeCallback callback(GetFilePath(), offset_, downloadSize_,
                                   stream_, cache, dirTree, this);

    if (async) {
      transferManager->GetExecutor()->SubmitAsync(
          bind(boost::type<void>(), callback, _1),
          bind(boost::type<shared_ptr<TransferHandle> >(),
               &QS::Client::TransferManager::DownloadFile,
               transferManager.get(), _1, offset_, downloadSize_, stream_,
               false),
          GetFilePath());
    } else {
      shared_ptr<TransferHandle> handle = transferManager->DownloadFile(
          GetFilePath(), offset_, downloadSize_, stream_);
      callback(handle);
    }

    downloadedSize += downloadSize_;
    remainingSize -= downloadSize_;
  }
}

// --------------------------------------------------------------------------
tuple<PageSetConstIterator, bool, size_t, size_t> File::UnguardedAddPage(
    off_t offset, size_t len, const char *buffer) {
  lock_guard<recursive_mutex> lock(m_mutex);

  // remove dummy page at first
  const char *dummy = "";
  PageSetConstIterator it =
      m_pages.find(shared_ptr<Page>(new Page(offset, 0, dummy)));
  if (it != m_pages.end()) {
    m_pages.erase(it);
  }

  pair<PageSetConstIterator, bool> res;
  size_t addedSize = 0;
  size_t addedSizeInCache = 0;
  if (UseDiskFile()) {
    res = m_pages.insert(
        make_shared<Page>(offset, len, buffer, AskDiskFilePath()));
  } else {
    res = m_pages.insert(shared_ptr<Page>(new Page(offset, len, buffer)));
    if (res.second) {
      addedSizeInCache = len;
      m_cacheSize += len;
    }
  }
  if (res.second) {
    addedSize = len;
    m_dataSize += len;
  } else {
    DebugError("Fail to new a page from a buffer " +
               ToStringLine(offset, len, buffer) + ToString());
  }

  return make_tuple(res.first, res.second, addedSizeInCache, addedSize);
}

// --------------------------------------------------------------------------
tuple<PageSetConstIterator, bool, size_t, size_t> File::UnguardedAddPage(
    off_t offset, size_t len, const shared_ptr<iostream> &stream) {
  lock_guard<recursive_mutex> lock(m_mutex);

  // remove dummy page at first
  const char *dummy = "";
  PageSetConstIterator it =
      m_pages.find(shared_ptr<Page>(new Page(offset, 0, dummy)));
  if (it != m_pages.end()) {
    m_pages.erase(it);
  }

  pair<PageSetConstIterator, bool> res;
  size_t addedSize = 0;
  size_t addedSizeInCache = 0;
  if (UseDiskFile()) {
    res = m_pages.insert(
        make_shared<Page>(offset, len, stream, AskDiskFilePath()));
  } else {
    res = m_pages.insert(shared_ptr<Page>(new Page(offset, len, stream)));
    if (res.second) {
      addedSizeInCache = len;
      m_cacheSize += len;
    }
  }
  if (res.second) {
    addedSize = len;
    m_dataSize += len;
  } else {
    DebugError("Fail to new a page from a stream " + ToStringLine(offset, len) +
               ToString());
  }
  return make_tuple(res.first, res.second, addedSizeInCache, addedSize);
}

}  // namespace Data
}  // namespace QS
