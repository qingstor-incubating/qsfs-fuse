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

#ifndef QSFS_DATA_NODE_H_
#define QSFS_DATA_NODE_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <set>
#include <string>

#include "boost/make_shared.hpp"
#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/unordered_map.hpp"
#include "boost/weak_ptr.hpp"

#include "base/HashUtils.h"
#include "data/Entry.h"

namespace QS {

namespace FileSystem {
class Drive;
struct UploadFileCallback;
}  // namespace FileSystem

namespace Data {

class Cache;
class DirectoryTree;
class Node;

typedef boost::unordered_map<std::string, boost::shared_ptr<Node>,
                             HashUtils::StringHash>
    FilePathToNodeUnorderedMap;
typedef FilePathToNodeUnorderedMap::iterator ChildrenMapIterator;
typedef FilePathToNodeUnorderedMap::const_iterator ChildrenMapConstIterator;


/**
 * Representation of a Node in the directory tree.
 */
class Node : private boost::noncopyable {
 public:
  // Ctor for root node which has no parent,
  // or for some case the parent is cleared or still not set at the time
  Node()
      : m_entry(Entry()),
        m_parent(boost::shared_ptr<Node>()),
        m_hardLink(false) {}

  Node(const Entry &entry,
       const boost::shared_ptr<Node> &parent = boost::make_shared<Node>());

  Node(const Entry &entry, const boost::shared_ptr<Node> &parent,
       const std::string &symbolicLink);

  // Not subclassing from enabled_shared_from_this, as cannot use
  // shared_from_this in ctors.
  // Node(Entry &&entry, boost::shared_ptr<Node> &parent);
  // Node(Entry &&entry, boost::shared_ptr<Node> &parent,
  //      const std::string &symbolicLink);

  ~Node();

 public:
  operator bool() const { return m_entry ? m_entry.operator bool() : false; }

  bool IsDirectory() const { return m_entry && m_entry.IsDirectory(); }
  bool IsSymLink() const { return m_entry && m_entry.IsSymLink(); }
  bool IsHardLink() const { return m_hardLink; }

 public:
  bool IsEmpty() const { return m_children.empty(); }
  bool HaveChild(const std::string &childFilePath) const;
  boost::shared_ptr<Node> Find(const std::string &childFilePath) const;

  // Get Children
  // DO NOT store the map
  const FilePathToNodeUnorderedMap &GetChildren() const;

  // Get the children's id (one level)
  std::set<std::string> GetChildrenIds() const;

  // Get the children file names recursively
  //
  // @param  : void
  // @return : a list of all children's file names and chilren's chidlren's ones
  //           in a recursively way. The nearest child is put at front.
  std::deque<std::string> GetChildrenIdsRecursively() const;

  boost::shared_ptr<Node> Insert(const boost::shared_ptr<Node> &child);
  void Remove(const boost::shared_ptr<Node> &child);
  void Remove(const std::string &childFilePath);
  void RenameChild(const std::string &oldFilePath,
                   const std::string &newFilePath);

  // accessor
  const Entry &GetEntry() const { return m_entry; }
  boost::shared_ptr<Node> GetParent() const { return m_parent.lock(); }
  std::string GetSymbolicLink() const { return m_symbolicLink; }

  std::string GetFilePath() const {
    return m_entry ? m_entry.GetFilePath() : std::string();
  }

  uint64_t GetFileSize() const { return m_entry ? m_entry.GetFileSize() : 0; }
  int GetNumLink() const { return m_entry ? m_entry.GetNumLink() : 0; }

  mode_t GetFileMode() const { return m_entry ? m_entry.GetFileMode() : 0; }
  time_t GetMTime() const { return m_entry ? m_entry.GetMTime() : 0; }
  time_t GetCachedTime() const { return m_entry ? m_entry.GetCachedTime() : 0; }
  uid_t GetUID() const { return m_entry ? m_entry.GetUID() : -1; }
  bool IsNeedUpload() const { return m_entry ? m_entry.IsNeedUpload() : false; }
  bool IsFileOpen() const { return m_entry ? m_entry.IsFileOpen() : false; }

  std::string MyDirName() const {
    return m_entry ? m_entry.MyDirName() : std::string();
  }
  std::string MyBaseName() const {
    return m_entry ? m_entry.MyBaseName() : std::string();
  }

  bool FileAccess(uid_t uid, gid_t gid, int amode) const {
    return m_entry ? m_entry.FileAccess(uid, gid, amode) : false;
  }

 private:
  Entry &GetEntry() { return m_entry; }

  FilePathToNodeUnorderedMap &GetChildren();

  void SetNeedUpload(bool needUpload) {
    if (m_entry) {
      m_entry.SetNeedUpload(needUpload);
    }
  }

  void SetFileOpen(bool fileOpen) {
    if (m_entry) {
      m_entry.SetFileOpen(fileOpen);
    }
  }

  void SetFileSize(size_t sz) {
    if (m_entry) {
      m_entry.SetFileSize(sz);
    }
  }

  void Rename(const std::string &newFilePath);

  void SetEntry(const Entry &entry) { m_entry = entry; }
  void SetParent(const boost::shared_ptr<Node> &parent) { m_parent = parent; }
  void SetSymbolicLink(const std::string &symLnk) { m_symbolicLink = symLnk; }
  void SetHardLink(bool isHardLink) { m_hardLink = isHardLink; }

  void IncreaseNumLink() {
    if (m_entry) {
      m_entry.IncreaseNumLink();
    }
  }

 private:
  Entry m_entry;
  boost::weak_ptr<Node> m_parent;
  std::string m_symbolicLink;
  bool m_hardLink;
  // Node will control the life of its children, so only Node hold a shared_ptr
  // to its children, others should use weak_ptr instead
  FilePathToNodeUnorderedMap m_children;

  friend class QS::Data::Cache;  // for GetEntry
  friend class QS::Data::DirectoryTree;
  friend class QS::FileSystem::Drive;  // for SetSymbolicLink, IncreaseNumLink
  friend struct QS::FileSystem::UploadFileCallback;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_NODE_H_
