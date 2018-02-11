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

#include "data/Entry.h"

#include <string>

#include "boost/shared_ptr.hpp"

#include "data/FileMetaData.h"
#include "data/FileMetaDataManager.h"

namespace QS {

namespace Data {

using boost::shared_ptr;
using std::string;



// --------------------------------------------------------------------------
Entry::Entry(const string &filePath, uint64_t fileSize, time_t atime,
             time_t mtime, uid_t uid, gid_t gid, mode_t fileMode,
             FileType::Value fileType, const string &mimeType,
             const string &eTag, bool encrypted, dev_t dev, int numlink) {
  // Seems make_shared not work when pass all args to construct FileMetaData
  shared_ptr<FileMetaData> meta = shared_ptr<FileMetaData>(
      new FileMetaData(filePath, fileSize, atime, mtime, uid, gid, fileMode,
                       fileType, mimeType, eTag, encrypted, dev, numlink));

  m_metaData = meta;
  FileMetaDataManager::Instance().Add(meta);
}

// --------------------------------------------------------------------------
Entry::Entry(const shared_ptr<FileMetaData> &fileMetaData)
    : m_metaData(fileMetaData) {
  FileMetaDataManager::Instance().Add(fileMetaData);
}

// --------------------------------------------------------------------------
void Entry::Rename(const string &newFilePath) {
  FileMetaDataManager::Instance().Rename(GetFilePath(), newFilePath);
}

}  // namespace Data
}  // namespace QS
