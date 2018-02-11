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

#include "data/FileMetaDataManager.h"

#include <string>
#include <utility>
#include <vector>

#include "boost/exception/to_string.hpp"
#include "boost/foreach.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"

#include "base/LogMacros.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "data/DirectoryTree.h"
#include "configure/Options.h"

namespace QS {

namespace Data {

using boost::lock_guard;
using boost::recursive_mutex;
using boost::shared_ptr;
using boost::to_string;
using QS::StringUtils::FormatPath;
using QS::Utils::GetDirName;
using QS::Utils::IsRootDirectory;
using std::pair;
using std::string;

// --------------------------------------------------------------------------
MetaDataListConstIterator FileMetaDataManager::Get(
    const string &filePath) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return const_cast<FileMetaDataManager *>(this)->Get(filePath);
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::Get(const string &filePath) {
  lock_guard<recursive_mutex> lock(m_mutex);
  MetaDataListIterator pos = m_metaDatas.end();
  FileIdToMetaDataMapIterator it = m_map.find(filePath);
  if (it != m_map.end()) {
    pos = UnguardedMakeMetaDataMostRecentlyUsed(it->second);
  } else {
    DebugInfo("File not exist " + FormatPath(filePath));
  }
  return pos;
}

// --------------------------------------------------------------------------
MetaDataListConstIterator FileMetaDataManager::Begin() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_metaDatas.begin();
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::Begin() {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_metaDatas.begin();
}

// --------------------------------------------------------------------------
MetaDataListConstIterator FileMetaDataManager::End() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_metaDatas.end();
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::End() {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_metaDatas.end();
}

// --------------------------------------------------------------------------
bool FileMetaDataManager::Has(const string &filePath) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return this->Get(filePath) != m_metaDatas.end();
}

// --------------------------------------------------------------------------
bool FileMetaDataManager::HasFreeSpace(size_t needCount) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_metaDatas.size() + needCount <= GetMaxCount();
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::AddNoLock(
    const shared_ptr<FileMetaData> &fileMetaData) {
  const string &filePath = fileMetaData->GetFilePath();
  FileIdToMetaDataMapIterator it = m_map.find(filePath);
  if (it == m_map.end()) {  // not exist in manager
    if (!HasFreeSpaceNoLock(1)) {
      bool success = FreeNoLock(1, filePath);
      if (!success) {
        m_maxCount += static_cast<size_t>(m_maxCount / 5);
        Warning("Enlarge max stat to " + to_string(m_maxCount));
      }
    }
    m_metaDatas.push_front(std::make_pair(filePath, fileMetaData));
    if (m_metaDatas.begin()->first == filePath) {  // insert sucessfully
      pair<FileIdToMetaDataMapIterator, bool> res =
          m_map.emplace(filePath, m_metaDatas.begin());
      if(res.second) {
        return m_metaDatas.begin();
      } else {
        DebugWarning("Fail to add file "+ FormatPath(filePath));
        return m_metaDatas.end();
      }
    } else {
      DebugWarning("Fail to add file " + FormatPath(filePath));
      return m_metaDatas.end();
    }
  } else {  // exist already, update it
    MetaDataListIterator pos =
        UnguardedMakeMetaDataMostRecentlyUsed(it->second);
    pos->second = fileMetaData;
    return pos;
  }
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::Add(
    const shared_ptr<FileMetaData> &fileMetaData) {
  lock_guard<recursive_mutex> lock(m_mutex);
  return AddNoLock(fileMetaData);
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::Add(
    const std::vector<shared_ptr<FileMetaData> > &fileMetaDatas) {
  lock_guard<recursive_mutex> lock(m_mutex);
  MetaDataListIterator pos = m_metaDatas.end();
  BOOST_FOREACH(const shared_ptr<FileMetaData> &meta, fileMetaDatas) {
    pos = AddNoLock(meta);
    if (pos == m_metaDatas.end()) break;  // if fail to add an item
  }
  return pos;
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::Erase(const string &filePath) {
  lock_guard<recursive_mutex> lock(m_mutex);
  MetaDataListIterator next = m_metaDatas.end();
  FileIdToMetaDataMapIterator it = m_map.find(filePath);
  if (it != m_map.end()) {
    next = m_metaDatas.erase(it->second);
    m_map.erase(it);
  } else {
    DebugWarning("File not exist, no remove " + FormatPath(filePath));
  }
  return next;
}

// --------------------------------------------------------------------------
void FileMetaDataManager::Clear() {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_map.clear();
  m_metaDatas.clear();
}

// --------------------------------------------------------------------------
void FileMetaDataManager::Rename(const string &oldFilePath,
                                 const string &newFilePath) {
  if (oldFilePath == newFilePath) {
    // Disable following info
    // DebugInfo("File exist, no rename" + FormatPath(newFilePath) );
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  if (m_map.find(newFilePath) != m_map.end()) {
    DebugWarning("File exist, no rename " +
                 FormatPath(oldFilePath, newFilePath));
    return;
  }

  FileIdToMetaDataMapIterator it = m_map.find(oldFilePath);
  if (it != m_map.end()) {
    it->second->first = newFilePath;
    it->second->second->m_filePath = newFilePath;
    MetaDataListIterator pos =
        UnguardedMakeMetaDataMostRecentlyUsed(it->second);
    pair<FileIdToMetaDataMapIterator, bool> res =
        m_map.emplace(newFilePath, pos);
    if (!res.second) {
      DebugWarning("Fail to rename " + FormatPath(oldFilePath, newFilePath));
    }
    m_map.erase(it);
  } else {
    DebugWarning("File not exist, no rename " + FormatPath(oldFilePath));
  }
}

// --------------------------------------------------------------------------
void FileMetaDataManager::SetDirectoryTree(QS::Data::DirectoryTree *tree) {
    lock_guard<recursive_mutex> lock(m_mutex);
    m_dirTree = tree;
}

// --------------------------------------------------------------------------
MetaDataListIterator FileMetaDataManager::UnguardedMakeMetaDataMostRecentlyUsed(
    MetaDataListIterator pos) {
  m_metaDatas.splice(m_metaDatas.begin(), m_metaDatas, pos);
  // no iterators or references become invalidated, so no need to update m_map.
  return m_metaDatas.begin();
}

// --------------------------------------------------------------------------
bool FileMetaDataManager::HasFreeSpaceNoLock(size_t needCount) const {
  return m_metaDatas.size() + needCount <= GetMaxCount();
}

// --------------------------------------------------------------------------
bool FileMetaDataManager::FreeNoLock(size_t needCount, string fileUnfreeable) {
  if (needCount > GetMaxCount()) {
    DebugError("Try to free file meta data manager of " + to_string(needCount) +
               " items which surpass the maximum file meta data count (" +
               to_string(GetMaxCount()) + "). Do nothing");
    return false;
  }
  if (HasFreeSpaceNoLock(needCount)) {
    // DebugInfo("Try to free file meta data manager of " + to_string(needCount)
    // + " items while free space is still availabe. Go on");
    return true;
  }

  assert(!m_metaDatas.empty());
  size_t freedCount = 0;
  // free all in once
  MetaDataList::reverse_iterator it = m_metaDatas.rbegin();
  while (it !=m_metaDatas.rend() && !HasFreeSpaceNoLock(needCount)) {
    string fileId = it->first;
    if (IsRootDirectory(fileId)) {
      // cannot free root
      ++it;
      continue;
    }
    if (it->second) {
      if (it->second->IsFileOpen() || it->second->IsNeedUpload()) {
        ++it;
        continue;
      }
      if(fileId[fileId.size() - 1] == '/') {  // do not free dir
        ++it;
        continue;
      }
      if (fileId == fileUnfreeable) {
        ++it;
        continue;
      }
      if(GetDirName(fileId) == GetDirName(fileUnfreeable)) {
        ++it;
        continue;
      }
    } else {
      DebugWarning("file metadata null" + FormatPath(fileId));
    }

    DebugInfo("Free file " + FormatPath(fileId)); 
    // Must invoke callback to update directory tree before erasing,
    // as directory node depend on the file meta data
    if (m_dirTree) {
      m_dirTree->Remove(fileId);
    }    
    // Node destructor will inovke FileMetaDataManger::Erase,
    // so double checking before earsing file meta
    FileIdToMetaDataMapIterator p = m_map.find(fileId);
    if(p != m_map.end()) {
      m_metaDatas.erase(p->second);
      m_map.erase(p);
    }

    ++freedCount;
  }

  if (HasFreeSpaceNoLock(needCount)) {
    return true;
  } else {
    Warning("Fail to free " + to_string(needCount) +
            " items for file " + FormatPath(fileUnfreeable));
    return false;
  }
}

// --------------------------------------------------------------------------
FileMetaDataManager::FileMetaDataManager() {
  m_maxCount = static_cast<size_t>(
      QS::Configure::Options::Instance().GetMaxStatCountInK() * QS::Size::K1);
  m_dirTree = NULL;
}

}  // namespace Data
}  // namespace QS
