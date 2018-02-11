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

#include "data/Node.h"

#include <assert.h>

#include <deque>
#include <set>
#include <string>
#include <utility>

#include "boost/foreach.hpp"
#include "boost/shared_ptr.hpp"

#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "data/FileMetaDataManager.h"

namespace QS {

namespace Data {

using boost::shared_ptr;
using QS::StringUtils::FormatPath;
using QS::Utils::AppendPathDelim;
using std::deque;
using std::pair;
using std::set;
using std::string;

// --------------------------------------------------------------------------
Node::Node(const Entry &entry, const shared_ptr<Node> &parent)
    : m_entry(entry), m_parent(parent), m_hardLink(false) {
  m_children.clear();
}

// --------------------------------------------------------------------------
Node::Node(const Entry &entry, const shared_ptr<Node> &parent,
           const string &symbolicLink)
    : m_entry(entry), m_parent(parent), m_hardLink(false) {
  // must use m_entry instead of entry which is moved to m_entry now
  if (m_entry && m_entry.GetFileSize() <= symbolicLink.size()) {
    m_symbolicLink = string(symbolicLink, 0, m_entry.GetFileSize());
  }
}

// --------------------------------------------------------------------------
Node::~Node() {
  if (!m_entry) return;

  if (IsDirectory() || IsHardLink()) {
    shared_ptr<Node> parent = m_parent.lock();
    if (parent) {
      parent->GetEntry().DecreaseNumLink();
    }
  }

  GetEntry().DecreaseNumLink();
  if (m_entry.GetNumLink() <= 0 ||
      (m_entry.GetNumLink() <= 1 && m_entry.IsDirectory())) {
    FileMetaDataManager::Instance().Erase(GetFilePath());
  }
}

// --------------------------------------------------------------------------
shared_ptr<Node> Node::Find(const string &childFileName) const {
  ChildrenMapConstIterator child = m_children.find(childFileName);
  if (child != m_children.end()) {
    return child->second;
  }
  return shared_ptr<Node>();
}

// --------------------------------------------------------------------------
bool Node::HaveChild(const string &childFilePath) const {
  return m_children.find(childFilePath) != m_children.end();
}

// --------------------------------------------------------------------------
const FilePathToNodeUnorderedMap &Node::GetChildren() const {
  return m_children;
}

// --------------------------------------------------------------------------
FilePathToNodeUnorderedMap &Node::GetChildren() { return m_children; }

// --------------------------------------------------------------------------
set<string> Node::GetChildrenIds() const {
  set<string> ids;
  BOOST_FOREACH(const FilePathToNodeUnorderedMap::value_type &p, m_children) {
    ids.insert(p.first);
  }
  return ids;
}

// --------------------------------------------------------------------------
deque<string> Node::GetChildrenIdsRecursively() const {
  deque<string> ids;
  deque<shared_ptr<Node> > childs;

  BOOST_FOREACH(const FilePathToNodeUnorderedMap::value_type &p, m_children) {
    ids.push_back(p.first);
    childs.push_back(p.second);
  }

  while (!childs.empty()) {
    shared_ptr<Node> &child = childs.front();
    childs.pop_front();

    if (child->IsDirectory()) {
      BOOST_FOREACH(const FilePathToNodeUnorderedMap::value_type &p,
                     child->GetChildren()) {
        ids.push_back(p.first);
        childs.push_back(p.second);
      }
    }
  }

  return ids;
}

// --------------------------------------------------------------------------
shared_ptr<Node> Node::Insert(const shared_ptr<Node> &child) {
  assert(IsDirectory());
  if (child) {
    pair<ChildrenMapIterator, bool> res =
        m_children.emplace(child->GetFilePath(), child);
    if (res.second) {
      if (child->IsDirectory()) {
        m_entry.IncreaseNumLink();
      }
    } else {
      DebugWarning("Node insertion failed " +
                FormatPath(child->GetFilePath()));
    }
  } else {
    DebugWarning("Try to insert null Node");
  }
  return child;
}

// --------------------------------------------------------------------------
void Node::Remove(const shared_ptr<Node> &child) {
  if (child) {
    Remove(child->GetFilePath());
  } else {
    DebugWarning("Try to remove null Node")
  }
}

// --------------------------------------------------------------------------
void Node::Remove(const string &childFilePath) {
  if (childFilePath.empty()) return;

  bool reset = m_children.size() == 1 ? true : false;
  ChildrenMapIterator it = m_children.find(childFilePath);
  if (it != m_children.end()) {
    m_children.erase(it);
    if (reset) m_children.clear();
  } else {
    DebugWarning("Node not exist, no remove " + FormatPath(childFilePath));
  }
}

// --------------------------------------------------------------------------
void Node::Rename(const string &newFilePath) {
  if (m_entry) {
    string oldFilePath = m_entry.GetFilePath();
    if (oldFilePath == newFilePath) {
      return;
    }
    if (m_children.find(newFilePath) != m_children.end()) {
      DebugWarning("Cannot rename, target node already exist " +
                   FormatPath(oldFilePath, newFilePath));
      return;
    }

    m_entry.Rename(newFilePath);

    if (m_children.empty()) {
      return;
    }

    deque<shared_ptr<Node> > childs;
    BOOST_FOREACH(FilePathToNodeUnorderedMap::value_type &p, m_children) {
      childs.push_back(p.second);
    }
    m_children.clear();
    BOOST_FOREACH(shared_ptr<Node> &child, childs) {
      string newPath = AppendPathDelim(newFilePath) + child->MyBaseName();
      pair<ChildrenMapIterator, bool> res = m_children.emplace(newPath, child);
      if (res.second) {
        child->Rename(newPath);
      } else {
        DebugWarning("Node rename failed " +
                     FormatPath(child->GetFilePath()));
      }
    }
  }
}

// --------------------------------------------------------------------------
void Node::RenameChild(const string &oldFilePath, const string &newFilePath) {
  if (oldFilePath == newFilePath) {
    DebugInfo("Same file name, no rename " + FormatPath(oldFilePath));
    return;
  }

  if (m_children.find(newFilePath) != m_children.end()) {
    DebugWarning("Cannot rename, target node already exist " +
                 FormatPath(oldFilePath, newFilePath));
    return;
  }

  ChildrenMapIterator it = m_children.find(oldFilePath);
  if (it != m_children.end()) {
    shared_ptr<Node> &child = it->second;
    child->Rename(newFilePath);

    // Need to emplace before erase, otherwise the shared_ptr
    // will probably get deleted when erasing cause its' reference
    // count to be 0.
    pair<ChildrenMapIterator, bool> res = m_children.emplace(newFilePath, child);
    if (!res.second) {
      DebugWarning("Fail to rename " + FormatPath(oldFilePath, newFilePath));
    }
    m_children.erase(it);
  } else {
    DebugWarning("Node not exist, no rename " + FormatPath(oldFilePath));
  }
}

}  // namespace Data
}  // namespace QS
