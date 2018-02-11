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

#ifndef QSFS_DATA_DIRECTORYTREE_H_
#define QSFS_DATA_DIRECTORYTREE_H_

#include <time.h>

#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/unordered_map.hpp"
#include "boost/weak_ptr.hpp"

#include "base/HashUtils.h"


namespace QS {

namespace Client {
class QSClient;
}  // namespace Client

namespace FileSystem {
class Drive;
struct RenameDirCallback;
}  // namespace FileSystem

namespace Data {

class FileMetaData;
class FileMetaDataManager;
class Node;

typedef boost::unordered_map<std::string, boost::shared_ptr<Node>,
                             HashUtils::StringHash>
    FilePathToNodeUnorderedMap;
typedef FilePathToNodeUnorderedMap::iterator TreeNodeMapIterator;
typedef FilePathToNodeUnorderedMap::const_iterator TreeNodeMapConstIterator;

typedef boost::unordered_multimap<std::string, boost::weak_ptr<Node>,
                                  HashUtils::StringHash>
    ParentFilePathToChildrenMultiMap;
typedef ParentFilePathToChildrenMultiMap::iterator ChildrenMultiMapIterator;
typedef ParentFilePathToChildrenMultiMap::const_iterator
    ChildrenMultiMapConstIterator;


/**
 * Representation of the filesystem's directory tree.
 */
class DirectoryTree : private boost::noncopyable {
 public:
  DirectoryTree(time_t mtime, uid_t uid, gid_t gid, mode_t mode);

  ~DirectoryTree() { m_map.clear(); }

 public:
  // Get root
  boost::shared_ptr<Node> GetRoot() const;

  // Get current node
  // removed as this may has effect on remove
  // boost::shared_ptr<Node> GetCurrentNode() const;

  // Find node
  //
  // @param  : file path (absolute path)
  // @return : node
  boost::shared_ptr<Node> Find(const std::string &filePath) const;

  // Return if dir tree has node
  //
  // @param  : file path (absolute path)
  // @return : node
  bool Has(const std::string &filePath) const;

  // Find children
  //
  // @param  : dir name which should be ending with "/"
  // @return : node list
  // Notes: FindChildren do not find children recursively
  std::vector<boost::weak_ptr<Node> > FindChildren(
      const std::string &dirName) const;

  // Const iterator point to begin of the parent to children map
  ChildrenMultiMapConstIterator CBeginParentToChildrenMap() const;

  // Const iterator point to end of the parent to children map
  ChildrenMultiMapConstIterator CEndParentToChildrenMap() const;

 private:
  // Grow the directory tree
  //
  // @param  : file meta data
  // @return : the Node referencing to the file meta data
  //
  // If the node reference to the meta data already exist, update meta data;
  // otherwise add node to the tree and build up the references.
  boost::shared_ptr<Node> Grow(const boost::shared_ptr<FileMetaData> &fileMeta);

  // Grow the directory tree
  //
  // @param  : file meta data list
  // @return : void
  //
  // This will walk through the meta data list to Grow the dir tree.
  void Grow(const std::vector<boost::shared_ptr<FileMetaData> > &fileMetas);

  // Update a directory node in the directory tree
  //
  // @param  : dirpath, meta data of children
  // @return : the node has been update or null if update doesn't happen
  boost::shared_ptr<Node> UpdateDirectory(
      const std::string &dirPath,
      const std::vector<boost::shared_ptr<FileMetaData> > &childrenMetas);

  // Rename node
  //
  // @param  : old file path, new file path (absolute path)
  // @return : the node has been renamed or null if rename doesn't happen
  boost::shared_ptr<Node> Rename(const std::string &oldFilePath,
                                 const std::string &newFilePath);

  // Remove node
  //
  // @param  : path
  // @return : void
  //
  // This will remove node and all its childrens (recursively)
  void Remove(const std::string &path);

 private:
  DirectoryTree() {}

  boost::shared_ptr<Node> m_root;
  // boost::shared_ptr<Node> m_currentNode;
  mutable boost::recursive_mutex m_mutex;
  FilePathToNodeUnorderedMap m_map;  // record all nodes map

  // As we grow directory tree gradually, that means the directory tree can
  // be a partial part of the entire tree, at some point some nodes haven't
  // built the reference to its parent or children because which have not been
  // added to the tree yet.
  // So, the dirName to children map which will help to update these references.
  ParentFilePathToChildrenMultiMap m_parentToChildrenMap;

  friend class QS::Client::QSClient;
  friend class QS::Data::FileMetaDataManager;
  friend class QS::FileSystem::Drive;
  friend struct QS::FileSystem::RenameDirCallback;
  friend class DirectoryTreeTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_DIRECTORYTREE_H_
