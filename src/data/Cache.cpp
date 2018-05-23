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

#include "data/Cache.h"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "boost/exception/to_string.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"

#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "base/UtilsWithLog.h"
#include "configure/Options.h"
#include "data/DirectoryTree.h"
#include "data/File.h"
#include "data/Node.h"

namespace QS {

namespace Data {

using boost::lock_guard;
using boost::make_shared;
using boost::mutex;
using boost::recursive_mutex;
using boost::shared_ptr;
using boost::to_string;
using QS::StringUtils::FormatPath;
using QS::UtilsWithLog::IsSafeDiskSpace;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

// --------------------------------------------------------------------------
bool Cache::HasFreeSpace(size_t size) const {
  return GetSize() + size <= GetCapacity();
}

// --------------------------------------------------------------------------
bool Cache::HasFile(const string &filePath) const {
  lock_guard<recursive_mutex> locker(m_mutex);
  return m_map.find(filePath) != m_map.end();
}

// --------------------------------------------------------------------------
size_t Cache::GetNumFile() const {
  lock_guard<recursive_mutex> locker(m_mutex);
  return m_map.size();
}

// --------------------------------------------------------------------------
uint64_t Cache::GetSize() const {
  lock_guard<recursive_mutex> locker(m_mutex);
  return m_size;
}

// --------------------------------------------------------------------------
shared_ptr<File> Cache::FindFile(const string &filePath) {
  lock_guard<recursive_mutex> locker(m_mutex);
  CacheMapIterator it = m_map.find(filePath);
  CacheListIterator pos;
  if (it != m_map.end()) {
    pos = UnguardedMakeFileMostRecentlyUsed(it->second);
    return pos->second;
  } else {
    return shared_ptr<File>();
  }
}

// --------------------------------------------------------------------------
CacheListIterator Cache::Begin() {
  lock_guard<recursive_mutex> locker(m_mutex);
  return m_cache.begin();
}

// --------------------------------------------------------------------------
CacheListIterator Cache::End() {
  lock_guard<recursive_mutex> locker(m_mutex);
  return m_cache.end();
}

// --------------------------------------------------------------------------
boost::shared_ptr<File> Cache::MakeFile(const string &fileId) {
  lock_guard<recursive_mutex> locker(m_mutex);
  shared_ptr<File> f = FindFile(fileId);
  if (f) {
    return f;
  } else {
    CacheListIterator pos = UnguardedNewEmptyFile(fileId);
    if (pos != m_cache.end()) {
      return pos->second;
    } else {
      return shared_ptr<File>();
    }
  }
}

// --------------------------------------------------------------------------
bool Cache::Free(size_t size, const string &fileUnfreeable) {
  lock_guard<recursive_mutex> locker(m_mutex);
  if (size > GetCapacity()) {
    DebugInfo("Try to free cache of " + to_string(size) +
              " bytes which surpass the maximum cache size(" +
              to_string(GetCapacity()) + " bytes). Do nothing");
    return false;
  }
  if (HasFreeSpace(size)) {
    // DebugInfo("Try to free cache of " + to_string(size) +
    //           " bytes while free space is still available. Go on");
    return true;
  }

  assert(!m_cache.empty());
  size_t freedSpace = 0;
  size_t freedDiskSpace = 0;

  CacheList::reverse_iterator it = m_cache.rbegin();
  // Discards the least recently used File first, which is put at back.
  while (it != m_cache.rend() && !HasFreeSpace(size)) {
    // Notice do NOT store a reference of the File supposed to be removed.
    string fileId = it->first;
    if (fileId != fileUnfreeable && it->second && !it->second->IsOpen()) {
      size_t fileCacheSz = it->second->GetCachedSize();
      freedSpace += fileCacheSz;
      freedDiskSpace += it->second->GetDataSize() - fileCacheSz;
      SubtractSize(fileCacheSz);
      it->second->Clear();
      m_cache.erase((++it).base());
      m_map.erase(fileId);
    } else {
      if (!it->second) {
        DebugInfo("file in cache is null " + FormatPath(fileId));
        m_cache.erase((++it).base());
        m_map.erase(fileId);
      } else {
        ++it;
      }
    }
  }

  if (freedSpace > 0) {
    Info("Has freed cache of " + to_string(freedSpace) + " bytes for file " +
         FormatPath(fileUnfreeable));
  }
  if (freedDiskSpace > 0) {
    Info("Has freed disk file of " + to_string(freedDiskSpace) +
         " bytes for file " + FormatPath(fileUnfreeable));
  }
  return HasFreeSpace(size);
}

// --------------------------------------------------------------------------
bool Cache::FreeDiskCacheFiles(const string &diskfolder, size_t size,
                               const string &fileUnfreeable) {
  assert(diskfolder ==
         QS::Configure::Options::Instance().GetDiskCacheDirectory());
  lock_guard<recursive_mutex> locker(m_mutex);
  // diskfolder should be cache disk dir
  if (IsSafeDiskSpace(diskfolder, size)) {
    return true;
  }

  assert(!m_cache.empty());
  size_t freedSpace = 0;
  size_t freedDiskSpace = 0;

  CacheList::reverse_iterator it = m_cache.rbegin();
  // Discards the least recently used File first, which is put at back.
  while (it != m_cache.rend() && !IsSafeDiskSpace(diskfolder, size)) {
    // Notice do NOT store a reference of the File supposed to be removed.
    string fileId = it->first;
    if (fileId != fileUnfreeable && it->second && !it->second->IsOpen()) {
      size_t fileCacheSz = it->second->GetCachedSize();
      freedSpace += fileCacheSz;
      freedDiskSpace += it->second->GetDataSize() - fileCacheSz;
      SubtractSize(fileCacheSz);
      it->second->Clear();
      m_cache.erase((++it).base());
      m_map.erase(fileId);
    } else {
      if (!it->second) {
        DebugInfo("file in cache is null " + FormatPath(fileId));
        m_cache.erase((++it).base());
        m_map.erase(fileId);
      } else {
        ++it;
      }
    }
  }

  if (freedSpace > 0) {
    Info("Has freed cache of " + to_string(freedSpace) + " bytes for file " +
         FormatPath(fileUnfreeable));
  }
  if (freedDiskSpace > 0) {
    Info("Has freed disk file of " + to_string(freedDiskSpace) +
         " bytes for file" + FormatPath(fileUnfreeable));
  }
  return IsSafeDiskSpace(diskfolder, size);
}

// --------------------------------------------------------------------------
CacheListIterator Cache::Erase(const string &fileId) {
  lock_guard<recursive_mutex> locker(m_mutex);
  CacheMapIterator it = m_map.find(fileId);
  if (it != m_map.end()) {
    DebugInfo("Erase cache " + FormatPath(fileId));
    return UnguardedErase(it);
  } else {
    DebugInfo("File not exist, no remove " + FormatPath(fileId));
    return m_cache.end();
  }
}

// --------------------------------------------------------------------------
void Cache::Rename(const string &oldFileId, const string &newFileId) {
  if (oldFileId == newFileId) {
    DebugInfo("File exists, no rename " + FormatPath(oldFileId));
    return;
  }
  lock_guard<recursive_mutex> locker(m_mutex);
  CacheMapIterator iter = m_map.find(newFileId);
  if (iter != m_map.end()) {
    DebugWarning("File exist, Just remove it from cache " +
                 FormatPath(newFileId));
    UnguardedErase(iter);
  }

  CacheMapIterator it = m_map.find(oldFileId);
  if (it != m_map.end()) {
    it->second->first = newFileId;
    CacheListIterator pos = UnguardedMakeFileMostRecentlyUsed(it->second);
    pos->second->Rename(newFileId);

    pair<CacheMapIterator, bool> res = m_map.emplace(newFileId, pos);
    if (!res.second) {
      DebugWarning("Fail to rename " + FormatPath(oldFileId, newFileId));
    }
    m_map.erase(it);
    DebugInfo("Renamed file in cache" + FormatPath(oldFileId, newFileId));
  } else {
    DebugInfo("File not exists, no rename " + FormatPath(oldFileId, newFileId));
  }
}

// --------------------------------------------------------------------------
void Cache::MakeFileMostRecentlyUsed(const string &filePath) {
  lock_guard<recursive_mutex> locker(m_mutex);
  CacheMapIterator it = m_map.find(filePath);
  if (it != m_map.end()) {
    m_cache.splice(m_cache.begin(), m_cache, it->second);
  }
}

// --------------------------------------------------------------------------
void Cache::AddSize(uint64_t delta) {
  lock_guard<recursive_mutex> locker(m_mutex);
  m_size += delta;
}

// --------------------------------------------------------------------------
void Cache::SubtractSize(uint64_t delta) {
  lock_guard<recursive_mutex> locker(m_mutex);
  m_size -= delta;
}

// --------------------------------------------------------------------------
CacheListIterator Cache::UnguardedNewEmptyFile(const string &fileId) {
  m_cache.push_front(make_pair(fileId, make_shared<File>(fileId)));
  if (m_cache.begin()->first == fileId) {  // insert to cache sucessfully
    pair<CacheMapIterator, bool> res = m_map.emplace(fileId, m_cache.begin());
    if (res.second) {
      return m_cache.begin();
    } else {
      DebugError("Fail to create empty file in cache " + FormatPath(fileId));
      return m_cache.end();
    }
  } else {
    DebugError("Fail to create empty file in cache " + FormatPath(fileId));
    return m_cache.end();
  }
}

// --------------------------------------------------------------------------
CacheListIterator Cache::UnguardedErase(
    FileIdToCacheListIteratorMap::iterator pos) {
  CacheListIterator cachePos = pos->second;
  shared_ptr<File> &file = cachePos->second;
  SubtractSize(file->GetCachedSize());
  file->Clear();
  CacheListIterator next = m_cache.erase(cachePos);
  m_map.erase(pos);
  return next;
}

// --------------------------------------------------------------------------
CacheListIterator Cache::UnguardedMakeFileMostRecentlyUsed(
    CacheListIterator pos) {
  m_cache.splice(m_cache.begin(), m_cache, pos);
  // no iterators or references become invalidated, so no need to update m_map.
  return m_cache.begin();
}

}  // namespace Data
}  // namespace QS
