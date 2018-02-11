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

#include "data/DirectoryTree.h"

#include <assert.h>

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "boost/foreach.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/weak_ptr.hpp"

#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/TimeUtils.h"
#include "base/Utils.h"
#include "data/FileMetaData.h"
#include "data/Node.h"

namespace QS {

namespace Data {

using boost::lock_guard;
using boost::make_shared;
using boost::recursive_mutex;
using boost::shared_ptr;
using boost::weak_ptr;
using QS::StringUtils::FormatPath;
using QS::TimeUtils::SecondsToRFC822GMT;
using QS::Utils::AppendPathDelim;
using QS::Utils::IsRootDirectory;
using std::pair;
using std::queue;
using std::set;
using std::string;
using std::vector;

static const char *const ROOT_PATH = "/";

// --------------------------------------------------------------------------
shared_ptr<Node> DirectoryTree::GetRoot() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_root;
}

// --------------------------------------------------------------------------
// shared_ptr<Node> DirectoryTree::GetCurrentNode() const {
//   lock_guard<recursive_mutex> lock(m_mutex);
//   return m_currentNode;
// }

// --------------------------------------------------------------------------
shared_ptr<Node> DirectoryTree::Find(const string &filePath) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  TreeNodeMapConstIterator it = m_map.find(filePath);
  if (it != m_map.end()) {
    return it->second;
  } else {
    // Too many info, so disable it
    // DebugInfo("Node not exists in directory tree " + FormatPath(filePath));
    return shared_ptr<Node>();
  }
}

// --------------------------------------------------------------------------
bool DirectoryTree::Has(const string &filePath) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_map.find(filePath) != m_map.end();
}

// --------------------------------------------------------------------------
vector<weak_ptr<Node> > DirectoryTree::FindChildren(
    const string &dirName) const {
  lock_guard<recursive_mutex> lock(m_mutex);
  vector<weak_ptr<Node> > childs;
  for (pair<ChildrenMultiMapConstIterator, ChildrenMultiMapConstIterator>
           range = m_parentToChildrenMap.equal_range(dirName);
       range.first != range.second; ++range.first) {
    childs.push_back(range.first->second);
  }
  return childs;
}

// --------------------------------------------------------------------------
ChildrenMultiMapConstIterator DirectoryTree::CBeginParentToChildrenMap() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_parentToChildrenMap.cbegin();
}

// --------------------------------------------------------------------------
ChildrenMultiMapConstIterator DirectoryTree::CEndParentToChildrenMap() const {
  lock_guard<recursive_mutex> lock(m_mutex);
  return m_parentToChildrenMap.cend();
}

// --------------------------------------------------------------------------
shared_ptr<Node> DirectoryTree::Grow(const shared_ptr<FileMetaData> &fileMeta) {
  lock_guard<recursive_mutex> lock(m_mutex);
  if (!fileMeta) {
    return shared_ptr<Node>();
  }

  string filePath = fileMeta->GetFilePath();

  shared_ptr<Node> node = Find(filePath);
  if (node && *node) {
    time_t timein = fileMeta->GetMTime();
    time_t timecur = node->GetMTime();
    if (timein > timecur) {
      DebugInfo("Update node " + FormatPath(filePath));
      node->SetEntry(Entry(fileMeta));  // update entry
    } else if (timein < timecur) {
      if(!node->IsDirectory()) {
        DebugWarning("file mtime too old " + FormatPath(filePath) +
                     "[input mtime:" + SecondsToRFC822GMT(timein) +
                     ", current mtime:" + SecondsToRFC822GMT(timecur) + "]");
      }
    }
  } else {
    DebugInfo("Add node " + FormatPath(filePath));
    bool isDir = fileMeta->IsDirectory();
    string dirName = fileMeta->MyDirName();
    node = make_shared<Node>(Entry(fileMeta));
    pair<TreeNodeMapIterator, bool> res = m_map.emplace(filePath, node);
    if(!res.second) {
      DebugError("Fail to add node " + FormatPath(filePath));
      return shared_ptr<Node>();
    }

    // hook up with parent
    assert(!dirName.empty());
    TreeNodeMapIterator it = m_map.find(dirName);
    if (it != m_map.end()) {
      shared_ptr<Node> &parent = it->second;
      if (parent && *parent) {
        if (parent->HaveChild(filePath)) {
          parent->Remove(filePath);
        }
        parent->Insert(node);
        node->SetParent(parent);
      } else {
        DebugInfo("Parent node not exist " + FormatPath(filePath));
      }
    }

    // hook up with children
    if (isDir) {
      vector<weak_ptr<Node> > childs = FindChildren(filePath);
      BOOST_FOREACH(weak_ptr<Node> &child, childs) {
        shared_ptr<Node> childNode = child.lock();
        if (childNode && *childNode) {
          childNode->SetParent(node);
          node->Insert(childNode);
        }
      }
    }

    // record parent to children map
    if (m_parentToChildrenMap.emplace(dirName, node) ==
        m_parentToChildrenMap.end()) {
      DebugError("Fail to add node " + FormatPath(filePath));
      return shared_ptr<Node>();
    }
  }
  // m_currentNode = node;

  return node;
}

// --------------------------------------------------------------------------
void DirectoryTree::Grow(const vector<shared_ptr<FileMetaData> > &fileMetas) {
  lock_guard<recursive_mutex> lock(m_mutex);
  BOOST_FOREACH(const shared_ptr<FileMetaData> &meta, fileMetas) {
    Grow(meta);
  }
}

// --------------------------------------------------------------------------
shared_ptr<Node> DirectoryTree::UpdateDirectory(
    const string &dirPath,
    const vector<shared_ptr<FileMetaData> > &childrenMetas) {
  if (dirPath.empty()) {
    DebugWarning("Null dir path");
    return shared_ptr<Node>();
  }
  string path = dirPath;
  if (dirPath[dirPath.size() - 1] != '/') {
    DebugInfo("Input dir path is not ending with '/', append it");
    path = AppendPathDelim(dirPath);
  }

  DebugInfo("Update directory " + FormatPath(dirPath));
  lock_guard<recursive_mutex> lock(m_mutex);
  // Check children metas and collect valid ones
  vector<shared_ptr<FileMetaData> > newChildrenMetas;
  set<string> newChildrenIds;
  BOOST_FOREACH(const shared_ptr<FileMetaData> &child, childrenMetas) {
    string childDirName = child->MyDirName();
    string childFilePath = child->GetFilePath();
    if (childDirName.empty()) {
      DebugWarning("Invalid child Node " + FormatPath(childFilePath) +
                   " has empty dirname");
      continue;
    }
    if (childDirName != path) {
      DebugWarning("Invalid child Node " + FormatPath(childFilePath) +
                   " has different dir with " + path);
      continue;
    }
    newChildrenIds.insert(childFilePath);
    newChildrenMetas.push_back(child);
  }

  // Update
  shared_ptr<Node> node = Find(path);
  if (node && *node) {
    if (!node->IsDirectory()) {
      DebugWarning("Not a directory " + FormatPath(path));
      return shared_ptr<Node>();
    }

    // Do deleting
    set<string> oldChildrenIds = node->GetChildrenIds();
    set<string> deleteChildrenIds;
    std::set_difference(
        oldChildrenIds.begin(), oldChildrenIds.end(), newChildrenIds.begin(),
        newChildrenIds.end(),
        std::inserter(deleteChildrenIds, deleteChildrenIds.end()));
    if (!deleteChildrenIds.empty()) {
      vector<weak_ptr<Node> > childs = FindChildren(path);
      m_parentToChildrenMap.erase(path);
      BOOST_FOREACH(weak_ptr<Node> &child, childs) {
        shared_ptr<Node> childNode = child.lock();
        if (childNode && (*childNode)) {
          if (deleteChildrenIds.find(childNode->GetFilePath()) ==
              deleteChildrenIds.end()) {
            if (m_parentToChildrenMap.emplace(path, child) ==
                m_parentToChildrenMap.end()) {
                DebugWarning("Fail to update node " + FormatPath(path));
            }
          }
        }
      }
      BOOST_FOREACH(const string &childId, deleteChildrenIds) {
        Remove(childId);
      }
    }

    // Do updating
    Grow(newChildrenMetas);
  } else {  // directory not existing
    node = Grow(BuildDefaultDirectoryMeta(path));
    Grow(newChildrenMetas);
  }

  // m_currentNode = node;
  return node;
}

// --------------------------------------------------------------------------
shared_ptr<Node> DirectoryTree::Rename(const string &oldFilePath,
                                       const string &newFilePath) {
  if (oldFilePath.empty() || newFilePath.empty()) {
    DebugWarning("Cannot rename " + FormatPath(oldFilePath, newFilePath));
    return shared_ptr<Node>();
  }
  if (IsRootDirectory(oldFilePath)) {
    DebugWarning("Unable to rename root");
    return shared_ptr<Node>();
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  shared_ptr<Node> node = Find(oldFilePath);
  if (node && *node) {
    // Check parameter
    if (Find(newFilePath)) {
      DebugWarning("Node exist, no rename " + FormatPath(newFilePath));
      return node;
    }
    if (!(*node)) {
      DebugWarning("Node not operable, no rename " + FormatPath(oldFilePath));
      return node;
    }

    // Do Renaming
    DebugInfo("Rename node " + FormatPath(oldFilePath, newFilePath));
    string parentName = node->MyDirName();
    node->Rename(newFilePath);  // still need as parent maybe not added yet
    shared_ptr<Node> parent = node->GetParent();
    if (parent && *parent) {
      parent->RenameChild(oldFilePath, newFilePath);
    }
    pair<TreeNodeMapIterator, bool> res = m_map.emplace(newFilePath, node);
    if (!res.second) {
      DebugWarning("Fail to rename node " +
                   FormatPath(oldFilePath, newFilePath));
    }
    m_map.erase(oldFilePath);
    if (node->IsDirectory()) {
      bool fail = false;
      vector<weak_ptr<Node> > childs = FindChildren(oldFilePath);
      BOOST_FOREACH(weak_ptr<Node> &child, childs) {
        if(m_parentToChildrenMap.emplace(newFilePath, child) ==
           m_parentToChildrenMap.end()) {
            fail = true;
        }
      }
      if(fail) {
        DebugWarning("Fail to rename node " +
                      FormatPath(oldFilePath, newFilePath));
      }
      m_parentToChildrenMap.erase(oldFilePath);
    }
    // m_currentNode = node;
  } else {
    DebugWarning("Node not exist " + FormatPath(oldFilePath));
  }

  return node;
}

// --------------------------------------------------------------------------
void DirectoryTree::Remove(const string &path) {
  if (IsRootDirectory(path)) {
    DebugWarning("Unable to remove root");
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  shared_ptr<Node> node = Find(path);
  if (!(node && *node)) {
    DebugInfo("No such file or directory, no remove " + FormatPath(path));
    return;
  }

  DebugInfo("Remove node " + FormatPath(path));
  shared_ptr<Node> parent = node->GetParent();
  if (parent && *parent) {
    // if path is a directory, when go out of this function, destructor
    // will recursively delete all its children, as there is no references
    // to the node now.
    parent->Remove(path);
  }
  m_map.erase(path);
  string nodeDir = node->MyDirName();
  for (ChildrenMultiMapIterator it = m_parentToChildrenMap.begin();
       it != m_parentToChildrenMap.end();) {
    if (it->first == nodeDir) {
      shared_ptr<Node> n = it->second.lock();
      if (n && n->GetFilePath() == path) {
        // erase will invalidate iterator, so should not increment it after that
        it = m_parentToChildrenMap.erase(it);
        break;
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
  m_parentToChildrenMap.erase(path);

  if (!node->IsDirectory()) {
    // Do not need to reset, destructor will be invoked at end
    return;
  }

  std::queue<shared_ptr<Node> > deleteNodes;
  BOOST_FOREACH(const FilePathToNodeUnorderedMap::value_type &p,
                 node->GetChildren()) {
    deleteNodes.push(p.second);
  }
  // recursively remove all children references
  while (!deleteNodes.empty()) {
    shared_ptr<Node> node_ = deleteNodes.front();
    deleteNodes.pop();

    string path_ = node_->GetFilePath();
    m_map.erase(path_);
    m_parentToChildrenMap.erase(path_);

    if (node->IsDirectory()) {
      BOOST_FOREACH(const FilePathToNodeUnorderedMap::value_type &p,
                     node_->GetChildren()) {
        deleteNodes.push(p.second);
      }
    }
  }
}

// --------------------------------------------------------------------------
DirectoryTree::DirectoryTree(time_t mtime, uid_t uid, gid_t gid, mode_t mode) {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_root = make_shared<Node>(
      Entry(ROOT_PATH, 0, mtime, mtime, uid, gid, mode, FileType::Directory));
  // m_currentNode = m_root;
  m_root->SetFileOpen(true);  // always keep root open
  m_map.emplace(ROOT_PATH, m_root);
}

}  // namespace Data
}  // namespace QS
