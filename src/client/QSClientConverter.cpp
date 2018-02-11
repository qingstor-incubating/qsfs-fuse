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

#define __STDC_LIMIT_MACROS  // for UINT64_MAX always put at first

#include "client/QSClientConverter.h"

#include <assert.h>
#include <stdint.h>  // for uint64_t
#include <time.h>

#include <sys/stat.h>  // for mode_t

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "boost/bind.hpp"
#include "boost/foreach.hpp"
#include "boost/lambda/lambda.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"

#include "qingstor/HttpCommon.h"

#include "base/TimeUtils.h"
#include "base/Utils.h"
#include "configure/Default.h"
#include "data/FileMetaData.h"
#include "filesystem/MimeTypes.h"

namespace QS {

namespace Client {

namespace QSClientConverter {

using boost::make_shared;
using boost::shared_ptr;
using QingStor::GetBucketStatisticsOutput;
using QingStor::HeadObjectOutput;
using QingStor::Http::HttpResponseCode;
using QingStor::ListObjectsOutput;
using QS::Data::BuildDefaultDirectoryMeta;
using QS::Data::FileMetaData;
using QS::Data::FileType;
using QS::Configure::Default::GetBlockSize;
using QS::Configure::Default::GetDefineFileMode;
using QS::Configure::Default::GetDefineDirMode;
using QS::Configure::Default::GetFragmentSize;
using QS::Configure::Default::GetNameMaxLen;
using QS::FileSystem::GetDirectoryMimeType;
using QS::FileSystem::GetSymlinkMimeType;
using QS::TimeUtils::RFC822GMTToSeconds;
using QS::Utils::AppendPathDelim;
using QS::Utils::GetProcessEffectiveUserID;
using QS::Utils::GetProcessEffectiveGroupID;
using std::string;
using std::vector;

namespace {
struct Predicate {
  string m_path;
  explicit Predicate(const string &path) : m_path(path) {}
  bool compare(const shared_ptr<FileMetaData> &meta) const {
    return meta->GetFilePath() == m_path;
  }
};

}  // namespace

// --------------------------------------------------------------------------
void GetBucketStatisticsOutputToStatvfs(
    const GetBucketStatisticsOutput &bucketStatsOutput, struct statvfs *statv) {
  assert(statv != NULL);
  if (statv == NULL) return;

  GetBucketStatisticsOutput &output =
      const_cast<GetBucketStatisticsOutput &>(bucketStatsOutput);
  uint64_t numObjs = output.GetCount();
  uint64_t bytesUsed = output.GetSize();
  uint64_t bytesTotal = UINT64_MAX;  // object storage is unlimited
  uint64_t bytesFree = bytesTotal - bytesUsed;

  statv->f_bsize = GetBlockSize();      // Filesystem block size
  statv->f_frsize = GetFragmentSize();  // Fragment size
  statv->f_blocks =
      (bytesTotal / statv->f_frsize);  // Size of fs in f_frsize units
  statv->f_bfree = (bytesFree / statv->f_frsize);  // Number of free blocks
  statv->f_bavail =
      statv->f_bfree;        // Number of free blocks for unprivileged users
  statv->f_files = numObjs;  // Number of inodes
  statv->f_namemax = GetNameMaxLen();  // Maximum filename length
}

// --------------------------------------------------------------------------
shared_ptr<FileMetaData> HeadObjectOutputToFileMetaData(
    const string &objKey, const HeadObjectOutput &headObjOutput) {
  HeadObjectOutput &output = const_cast<HeadObjectOutput &>(headObjOutput);
  if (output.GetResponseCode() == QingStor::Http::NOT_FOUND) {
    return shared_ptr<FileMetaData>();
  }

  uint64_t size = static_cast<uint64_t>(output.GetContentLength());

  // obey mime type for now, may need update in future,
  // as object storage has no dir concept,
  // a dir could have no application/x-directory mime type.
  string mimeType = output.GetContentType();
  bool isDir = mimeType == GetDirectoryMimeType();
  FileType::Value type = isDir ? FileType::Directory
                               : mimeType == GetSymlinkMimeType()
                                     ? FileType::SymLink
                                     : FileType::File;

  // TODO(jim): mode should do with meta when skd support this
  mode_t mode = isDir ? GetDefineDirMode() : GetDefineFileMode();

  // head object should contain meta such as mtime, but we just do a double
  // check as it can be have no meta data e.g when response code=NOT_MODIFIED
  string lastModified = output.GetLastModified();
  assert(!lastModified.empty());
  time_t atime = time(NULL);
  time_t mtime = lastModified.empty() ? 0 : RFC822GMTToSeconds(lastModified);
  bool encrypted = !output.GetXQSEncryptionCustomerAlgorithm().empty();
  return shared_ptr<FileMetaData>(
      new FileMetaData(objKey, size, atime, mtime, GetProcessEffectiveUserID(),
                       GetProcessEffectiveGroupID(), mode, type, mimeType,
                       output.GetETag(), encrypted));
}

// --------------------------------------------------------------------------
shared_ptr<FileMetaData> ObjectKeyToFileMetaData(const KeyType &objectKey,
                                                 time_t atime) {
  // Do const cast as sdk does not provide const-qualified accessors
  KeyType &key = const_cast<KeyType &>(objectKey);
  string fullPath = "/" + key.GetKey();  // build full path
  string mimeType = key.GetMimeType();
  bool isDir = mimeType == GetDirectoryMimeType();
  FileType::Value type = isDir ? FileType::Directory
                               : mimeType == GetSymlinkMimeType()
                                     ? FileType::SymLink
                                     : FileType::File;
  // TODO(jim): mode should do with meta when skd support this
  mode_t mode = isDir ? GetDefineDirMode() : GetDefineFileMode();
  return shared_ptr<FileMetaData>(new FileMetaData(
      fullPath, static_cast<uint64_t>(key.GetSize()), atime,
      static_cast<time_t>(key.GetModified()), GetProcessEffectiveUserID(),
      GetProcessEffectiveGroupID(), mode, type, mimeType, key.GetEtag(),
      key.GetEncrypted()));
}

// --------------------------------------------------------------------------
shared_ptr<FileMetaData> ObjectKeyToDirMetaData(const KeyType &objectKey,
                                                time_t atime) {
  // Do const cast as sdk does not provide const-qualified accessors
  KeyType &key = const_cast<KeyType &>(objectKey);
  string fullPath = AppendPathDelim("/" + key.GetKey());  // build full path

  return shared_ptr<FileMetaData>(new FileMetaData(
      fullPath, 0, atime, static_cast<time_t>(key.GetModified()),
      GetProcessEffectiveUserID(), GetProcessEffectiveGroupID(),
      GetDefineDirMode(), FileType::Directory, GetDirectoryMimeType(),
      key.GetEtag(), key.GetEncrypted()));
}

// --------------------------------------------------------------------------
shared_ptr<FileMetaData> CommonPrefixToFileMetaData(const string &commonPrefix,
                                                    time_t atime) {
  string fullPath = "/" + commonPrefix;
  // Walk aroud, as ListObject return no meta for a dir, so set mtime=0.
  // This is ok, as any update based on the condition that if dir is modified
  // should still be available.
  time_t mtime = 0;
  return make_shared<FileMetaData>(
      fullPath, 0, atime, mtime, GetProcessEffectiveUserID(),
      GetProcessEffectiveGroupID(), GetDefineDirMode(),
      FileType::Directory);  // TODO(jim): sdk api (meta)
}

// --------------------------------------------------------------------------
vector<shared_ptr<FileMetaData> > ListObjectsOutputToFileMetaDatas(
    const ListObjectsOutput &listObjsOutput, bool addSelf) {
  // Do const cast as sdk does not provide const-qualified accessors
  ListObjectsOutput &output = const_cast<ListObjectsOutput &>(listObjsOutput);
  vector<shared_ptr<FileMetaData> > metas;
  if (output.GetResponseCode() == QingStor::Http::NOT_FOUND) {
    return metas;
  }

  time_t atime = time(NULL);
  string prefix = output.GetPrefix();
  const KeyType *dirItselfAsKey = NULL;
  // Add files
  BOOST_FOREACH(const KeyType &key, output.GetKeys()) {
    // sdk will put dir (if exists) itself into keys, ignore it
    if (prefix == const_cast<KeyType &>(key).GetKey()) {
      dirItselfAsKey = &key;
      continue;
    }
    metas.push_back(ObjectKeyToFileMetaData(key, atime));
  }
  // Add subdirs
  BOOST_FOREACH(const string &commonPrefix, output.GetCommonPrefixes()) {
    metas.push_back(CommonPrefixToFileMetaData(commonPrefix, atime));
  }

  // Add dir itself
  if (addSelf) {
    string dirPath = AppendPathDelim("/" + prefix);
    Predicate p(dirPath);
    using boost::lambda::_1;
    if (std::find_if(metas.begin(), metas.end(),
                     boost::bind(boost::type<bool>(), &Predicate::compare, &p,
                                 _1)) == metas.end()) {
      if (dirItselfAsKey != NULL) {
        metas.push_back(ObjectKeyToDirMetaData(*dirItselfAsKey, atime));
      } else {
        metas.push_back(BuildDefaultDirectoryMeta(dirPath));  // mtime = 0
      }
    }
  }

  return metas;
}

}  // namespace QSClientConverter
}  // namespace Client
}  // namespace QS
