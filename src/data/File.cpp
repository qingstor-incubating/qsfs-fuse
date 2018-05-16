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
using QS::StringUtils::PointerAddress;
using QS::StringUtils::BoolToString;
using QS::StringUtils::ContentRangeDequeToString;
using QS::StringUtils::FormatPath;
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
      m_size(size),
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
      if (size == 0 && start <= static_cast<off_t>(m_size)) {
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
  if (size == 0 || m_size == 0 || m_pages.empty()) {
    return ranges;
  }

  off_t stop = static_cast<off_t>(start + size);
  pair<PageSetConstIterator, PageSetConstIterator> range =
      IntesectingRange(start, stop);

  if (range.first == range.second) {
    return ranges;
  }

  PageSetConstIterator cur = range.first;
  PageSetConstIterator next = range.first;
  while (++next != range.second) {
    if ((*cur)->Next() < (*next)->Offset()) {
      if ((*next)->Offset() > static_cast<off_t>(size)) {
        break;
      }
      off_t off = (*cur)->Next();
      size_t size = static_cast<size_t>((*next)->Offset() - off);
      ranges.push_back(make_pair(off, size));
    }
    ++cur;
  }

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
  return "[" + m_baseName + " size:" + to_string(m_size) +
         ", cachedsize:" + to_string(m_cacheSize) +
         ", useDisk:" + BoolToString(m_useDiskFile) +
         ", open:" + BoolToString(m_open) +
         ", pages:" + PageSetToString(m_pages) + "]";
}

// --------------------------------------------------------------------------
tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> File::Read(
    off_t offset, size_t len) {
  // Cache already check input.
  // bool isValidInput = offset >= 0 && len > 0 && entry != nullptr && (*entry);
  // assert(isValidInput);
  // if (!isValidInput) {
  //   DebugError("Fail to read file with invalid input " +
  //              ToStringLine(offset, len));
  //   return {0, list<shared_ptr<Page>>()};
  // }

  ContentRangeDeque unloadedRanges;
  size_t outcomeSize = 0;
  list<shared_ptr<Page> > outcomePages;

  if (len == 0) {
    unloadedRanges.push_back(make_pair(offset, len));
    return make_tuple(outcomeSize, outcomePages, unloadedRanges);
  }

  {
    lock_guard<recursive_mutex> lock(m_mutex);

    // If pages is empty.
    if (m_pages.empty()) {
      unloadedRanges.push_back(make_pair(offset, len));
      return make_tuple(outcomeSize, outcomePages, unloadedRanges);
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
          outcomePages.push_back(page);
          outcomeSize += page->m_size;
          return make_tuple(outcomeSize, outcomePages, unloadedRanges);
        } else {
          outcomePages.push_back(page);
          outcomeSize += page->m_size;

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
  }  // end of lock_guard

  return make_tuple(outcomeSize, outcomePages, unloadedRanges);
}

// --------------------------------------------------------------------------
tuple<bool, size_t, size_t> File::Write(off_t offset, size_t len,
                                        const char *buffer, bool open) {
  // Cache has checked input.
  // bool isValidInput = = offset >= 0 && len > 0 &&  buffer != NULL;
  // assert(isValidInput);
  // if (!isValidInput) {
  //   DebugError("Fail to write file with invalid input " +
  //              ToStringLine(offset, len, buffer));
  //   return false;
  // }

  lock_guard<recursive_mutex> lock(m_mutex);

  SetOpen(open);
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
        // refresh parital content of page
        return make_tuple(page->Refresh(offset_, len_, buffer + start_),
                          addedSizeInCache, addedSize);
      } else {
        // refresh entire page
        bool refresh = page->Refresh(buffer + start_);
        if (!refresh) {
          success = false;
          return make_tuple(false, addedSizeInCache, addedSize);
        }

        offset_ = page->Next();
        start_ += page->m_size;
        len_ -= page->m_size;
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
tuple<bool, size_t, size_t> File::Write(off_t offset, size_t len,
                                        const shared_ptr<iostream> &stream,
                                        bool open) {
  lock_guard<recursive_mutex> lock(m_mutex);
  SetOpen(open);
  if (m_pages.empty()) {
    tuple<PageSetConstIterator, bool, size_t, size_t> res =
        UnguardedAddPage(offset, len, stream);
    return make_tuple(boost::get<1>(res), boost::get<2>(res),
                      boost::get<3>(res));
  } else {
    PageSetConstIterator it = LowerBoundPage(offset);
    const shared_ptr<Page> &page = *it;
    if (it == m_pages.end()) {
      tuple<PageSetConstIterator, bool, size_t, size_t> res =
          UnguardedAddPage(offset, len, stream);
      return make_tuple(boost::get<1>(res), boost::get<2>(res),
                        boost::get<3>(res));
    } else if (page->Offset() == offset && page->Size() == len) {
      // replace old stream
      page->SetStream(stream);
      return make_tuple(true, 0, 0);
    } else {
      scoped_ptr<vector<char> > buf(new vector<char>(len));
      stream->seekg(0, std::ios_base::beg);
      stream->read(&(*buf)[0], len);

      return Write(offset, len, &(*buf)[0], open);
    }
  }
}

// --------------------------------------------------------------------------
struct FlushCallback {
  string filePath;
  uint64_t fileSize;
  shared_ptr<TransferManager> transferManager;
  shared_ptr<DirectoryTree> dirTree;
  shared_ptr<Cache> cache;
  shared_ptr<Client> client;
  bool releaseFile;
  bool updateMeta;

  FlushCallback(const string &filePath_, uint64_t fileSize_,
                const shared_ptr<TransferManager> &transferManager_,
                const shared_ptr<DirectoryTree> &dirTree_,
                const shared_ptr<Cache> &cache_,
                const shared_ptr<Client> &client_, bool releaseFile_,
                bool updateMeta_)
      : filePath(filePath_),
        fileSize(fileSize_),
        transferManager(transferManager_),
        dirTree(dirTree_),
        cache(cache_),
        client(client_),
        releaseFile(releaseFile_),
        updateMeta(updateMeta_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle && cache && client) {
      if (releaseFile) {
        // To gurantee the data consistency, file open state has been set in
        // Cache
        cache->SetFileOpen(filePath, false, dirTree);
        Info("Close file " + FormatPath(filePath));
      }

      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        Info("Done Upload file [size=" + to_string(fileSize) + "] " +
             FormatPath(filePath));
        // update meta
        if (updateMeta) {
          ClientError<QS::Client::QSError::Value> err =
              client->Stat(handle->GetObjectKey(), dirTree);
          ErrorIf(!QS::Client::IsGoodQSError(err),
                  QS::Client::GetMessageForQSError(err));
        }  // update Meta
      } else {
        if (handle->IsMultipart()) {
          transferManager->m_unfinishedMultipartUploadHandles.emplace(
              handle->GetObjectKey(), handle);
        }
      }    // Done Transfer
    }
  }
};

// --------------------------------------------------------------------------
void File::Flush(size_t fileSize, shared_ptr<TransferManager> transferManager,
                 shared_ptr<DirectoryTree> dirTree, shared_ptr<Cache> cache,
                 shared_ptr<Client> client, bool releaseFile, bool updateMeta,
                 bool async) {
  // download unloaded pages for file
  // this is need as user could open a file and edit a part of it,
  // but you need the completed file in order to upload it.
  Load(fileSize, transferManager, dirTree, cache, async);

  lock_guard<recursive_mutex> lock(m_mutex);
  FlushCallback callback(GetFilePath(), fileSize, transferManager, dirTree,
                         cache, client, releaseFile, updateMeta);
  if (async) {
    transferManager->GetExecutor()->SubmitAsync(
        bind(boost::type<void>(), callback, _1),
        bind(boost::type<shared_ptr<TransferHandle> >(),
             &QS::Client::TransferManager::UploadFile, transferManager.get(),
             _1, fileSize, cache, false),
        GetFilePath());
  } else {
    callback(transferManager->UploadFile(GetFilePath(), fileSize, cache));
  }
}

// --------------------------------------------------------------------------
void File::Load(size_t fileSize, shared_ptr<TransferManager> transferManager,
                shared_ptr<DirectoryTree> dirTree, shared_ptr<Cache> cache,
                bool async) {
  lock_guard<recursive_mutex> lock(m_mutex);
  ContentRangeDeque ranges = GetUnloadedRanges(0, fileSize);
  if (!ranges.empty()) {
    DebugInfo("Download unloaded ranges:" + ContentRangeDequeToString(ranges) +
              " file:" + ToString());

    DownloadRanges(ranges, transferManager, dirTree, cache, async);
  }
}

// --------------------------------------------------------------------------
void File::ResizeToSmallerSize(size_t smallerSize) {
  size_t curSize = GetSize();
  if (smallerSize == curSize) {
    return;
  }
  if (smallerSize > curSize) {
    DebugWarning("File size: " + to_string(curSize) +
                 ", target size: " + to_string(smallerSize) +
                 ". Unable to resize File to a larger size. " +
                 PrintFileName(m_baseName));
    return;
  }

  {
    lock_guard<recursive_mutex> lock(m_mutex);

    while (!m_pages.empty() && smallerSize < m_size) {
      PageSetConstIterator lastPage = --m_pages.end();
      size_t lastPageSize = (*lastPage)->Size();
      if (smallerSize + lastPageSize <= m_size) {
        if (!(*lastPage)->UseDiskFile()) {
          m_cacheSize -= lastPageSize;
        }
        m_size -= lastPageSize;
        m_pages.erase(lastPage);
      } else {
        size_t newSize = lastPageSize - (m_size - smallerSize);
        // Do a lazy remove for last page.
        (*lastPage)->ResizeToSmallerSize(newSize);
        if (!(*lastPage)->UseDiskFile()) {
          m_cacheSize -= lastPageSize - newSize;
        }
        m_size -= lastPageSize - newSize;
        break;
      }
    }
  }
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
  m_size = 0;
  m_cacheSize = 0;
  RemoveDiskFileIfExists(true);
  m_useDiskFile = false;
}

// --------------------------------------------------------------------------
void File::Rename(const std::string &newFilePath) {
  lock_guard<mutex> locker(m_filePathLock);
  m_filePath = newFilePath;
  m_baseName = QS::Utils::GetBaseName(newFilePath);
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
  bool fileOpen;
  shared_ptr<Cache> cache;
  shared_ptr<DirectoryTree> dirTree;

  DownloadRangeCallback(const string &filePath_, off_t offset_,
                        size_t downloadSize_,
                        const shared_ptr<IOStream> &stream_, bool open_,
                        const shared_ptr<Cache> &cache_,
                        const shared_ptr<DirectoryTree> &dirTree_)
      : filePath(filePath_),
        offset(offset_),
        downloadSize(downloadSize_),
        stream(stream_),
        fileOpen(open_),
        cache(cache_),
        dirTree(dirTree_) {}

  void operator()(const shared_ptr<TransferHandle> &handle) {
    if (handle) {
      handle->WaitUntilFinished();
      if (handle->DoneTransfer() && !handle->HasFailedParts()) {
        // TODO (jim): write to file directly
        bool success = cache->Write(filePath, offset, downloadSize, stream,
                                    dirTree, fileOpen);
        ErrorIf(!success,
                "Fail to write cache [file:offset:len=" + filePath + ":" +
                    to_string(offset) + ":" + to_string(downloadSize) + "]");
      }
    }
  }
};

// --------------------------------------------------------------------------
void File::DownloadRanges(const ContentRangeDeque &ranges,
                          shared_ptr<TransferManager> transferManager,
                          shared_ptr<DirectoryTree> dirTree,
                          shared_ptr<Cache> cache, bool async) {
  BOOST_FOREACH (const ContentRangeDeque::value_type &range, ranges) {
    off_t offset = range.first;
    size_t size = range.second;
    bool fileContentExist = HasData(offset, size);
    if (fileContentExist) {
      return;
    }

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
      DownloadRangeCallback callback(GetFilePath(), offset_, downloadSize_,
                                     stream_, m_open, cache, dirTree);

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
}

// --------------------------------------------------------------------------
tuple<PageSetConstIterator, bool, size_t, size_t> File::UnguardedAddPage(
    off_t offset, size_t len, const char *buffer) {
  pair<PageSetConstIterator, bool> res;
  size_t addedSize = 0;
  size_t addedSizeInCache = 0;
  if (UseDiskFile()) {
    res = m_pages.insert(
        make_shared<Page>(offset, len, buffer, AskDiskFilePath()));
    // do not count size of data stored in disk file
  } else {
    res = m_pages.insert(shared_ptr<Page>(new Page(offset, len, buffer)));
    if (res.second) {
      addedSizeInCache = len;
      m_cacheSize += len;  // count size of data stored in cache
    }
  }
  if (res.second) {
    addedSize = len;
    m_size += len;
  } else {
    DebugError("Fail to new a page from a buffer " +
               ToStringLine(offset, len, buffer) + PrintFileName(m_baseName));
  }

  return make_tuple(res.first, res.second, addedSizeInCache, addedSize);
}

// --------------------------------------------------------------------------
tuple<PageSetConstIterator, bool, size_t, size_t> File::UnguardedAddPage(
    off_t offset, size_t len, const shared_ptr<iostream> &stream) {
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
    m_size += len;
  } else {
    DebugError("Fail to new a page from a stream " + ToStringLine(offset, len) +
               PrintFileName(m_baseName));
  }

  return make_tuple(res.first, res.second, addedSizeInCache, addedSize);
}

}  // namespace Data
}  // namespace QS
