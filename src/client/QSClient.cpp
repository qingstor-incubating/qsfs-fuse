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

#include "client/QSClient.h"

#include <assert.h>
#include <stdint.h>  // for uint64_t

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "qingstor/Bucket.h"
#include "qingstor/HttpCommon.h"
#include "qingstor/QingStor.h"
#include "qingstor/QsConfig.h"
#include "qingstor/QsSdkOption.h"
#include "qingstor/types/KeyType.h"
#include "qingstor/types/ObjectPartType.h"

#include "boost/bind.hpp"
#include "boost/foreach.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/once.hpp"

#include "base/LogMacros.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/TimeUtils.h"
#include "base/Utils.h"
#include "client/ClientConfiguration.h"
#include "client/ClientImpl.h"
#include "client/Constants.h"
#include "client/Protocol.h"
#include "client/QSClientConverter.h"
#include "client/QSClientImpl.h"
#include "client/QSClientOutcome.h"
#include "client/QSError.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/FileMetaData.h"
#include "data/Node.h"
#include "filesystem/MimeTypes.h"

namespace QS {

namespace Client {

using boost::bind;
using boost::call_once;
using boost::make_shared;
using boost::shared_ptr;
using QingStor::AbortMultipartUploadInput;
using QingStor::Bucket;
using QingStor::CompleteMultipartUploadInput;
using QingStor::GetObjectInput;
using QingStor::GetObjectOutput;
using QingStor::HeadObjectInput;
using QingStor::HeadObjectOutput;
using QingStor::Http::HttpResponseCode;
using QingStor::InitiateMultipartUploadInput;
using QingStor::InitiateMultipartUploadOutput;
using QingStor::ListObjectsInput;
using QingStor::ListObjectsOutput;
using QingStor::PutObjectInput;
using QingStor::QingStorService;
using QingStor::QsConfig;  // sdk config
using QingStor::UploadMultipartInput;
using QS::Data::BuildDefaultDirectoryMeta;
using QS::Data::Cache;
using QS::Data::DirectoryTree;
using QS::Data::FileMetaData;
using QS::Data::Node;
using QS::FileSystem::GetDirectoryMimeType;
using QS::FileSystem::GetSymlinkMimeType;
using QS::FileSystem::LookupMimeType;
using QS::StringUtils::FormatPath;
using QS::StringUtils::LTrim;
using QS::StringUtils::RTrim;
using QS::TimeUtils::SecondsToRFC822GMT;
using QS::TimeUtils::RFC822GMTToSeconds;
using QS::Utils::AppendPathDelim;
using QS::Utils::GetBaseName;
using QS::Utils::GetDirName;
using QS::Utils::IsRootDirectory;
using std::iostream;
using std::string;
using std::stringstream;
using std::vector;

namespace {

// --------------------------------------------------------------------------
string BuildXQSSourceString(const string &objKey) {
  const ClientConfiguration &clientConfig = ClientConfiguration::Instance();
  // format: /bucket-name/object-key
  string ret = "/";
  ret.append(clientConfig.GetBucket());
  ret.append("/");
  ret.append(LTrim(objKey, '/'));
  return ret;
}

// --------------------------------------------------------------------------
const char *GetSDKLogDir() {
  static string logdir;
  const ClientConfiguration &clientConfig = ClientConfiguration::Instance();
  logdir = AppendPathDelim(clientConfig.GetClientLogDirectory());
  return logdir.c_str();
}

}  // namespace

// --------------------------------------------------------------------------
struct PrintErrorMsg {
  PrintErrorMsg() {}
  void operator()(const ClientError<QSError::Value> &err) {
    DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  }
};

static boost::once_flag onceFlagGetClientImpl = BOOST_ONCE_INIT;
static boost::once_flag onceFlagStartService = BOOST_ONCE_INIT;

// --------------------------------------------------------------------------
QSClient::QSClient() : Client() {
  StartQSService();
  InitializeClientImpl();
}

// --------------------------------------------------------------------------
QSClient::~QSClient() { CloseQSService(); }

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::HeadBucket() {
  HeadBucketOutcome outcome = GetQSClientImpl()->HeadBucket();

  if (outcome.IsSuccess()) {
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
// DeleteFile is used to delete a file or an empty directory.
ClientError<QSError::Value> QSClient::DeleteFile(
    const string &filePath, const shared_ptr<DirectoryTree> &dirTree,
    const shared_ptr<Cache> &cache) {
  assert(dirTree && cache);
  shared_ptr<Node> node = dirTree->Find(filePath);
  if (node && *node) {
    // In case of hard links, multiple node have the same file, do not delete
    // the file for a hard link.
    if (node->IsHardLink() ||
        (!node->IsDirectory() && node->GetNumLink() >= 2)) {
      dirTree->Remove(filePath);
      return ClientError<QSError::Value>(QSError::GOOD, false);
    }
  }

  DeleteObjectOutcome outcome = GetQSClientImpl()->DeleteObject(filePath);
  if (outcome.IsSuccess()) {
    dirTree->Remove(filePath);

    if (cache && cache->HasFile(filePath)) {
      cache->Erase(filePath);
    }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::MakeFile(const string &filePath) {
  PutObjectInput input;
  input.SetContentLength(0);  // create empty file
  input.SetContentType(LookupMimeType(filePath));

  PutObjectOutcome outcome = GetQSClientImpl()->PutObject(filePath, &input);

  if (outcome.IsSuccess()) {
    // As sdk doesn't return the created file meta data in PutObjectOutput,
    // So we cannot grow the directory tree here, instead we need to call
    // Stat to head the object again in Drive::MakeFile;
    //
    // auto &drive = Drive::Instance();
    // auto &dirTree = Drive::Instance().GetDirectoryTree();
    // if (dirTree) {
    //   dirTree->Grow(PutObjectOutputToFileMeta());  // no implementation
    // }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::MakeDirectory(const string &dirPath) {
  PutObjectInput input;
  input.SetContentLength(0);  // directory has zero length
  input.SetContentType(GetDirectoryMimeType());
  string dir = AppendPathDelim(dirPath);

  PutObjectOutcome outcome = GetQSClientImpl()->PutObject(dir, &input);

  if (outcome.IsSuccess()) {
    // As sdk doesn't return the created file meta data in PutObjectOutput,
    // So we cannot grow the directory tree here, instead we need to call
    // Stat to head the object again in Drive::MakeDir;
    //
    // auto &drive = Drive::Instance();
    // auto &dirTree = Drive::Instance().GetDirectoryTree();
    // if (dirTree) {
    //   dirTree->Grow(PutObjectOutputToFileMeta());  // no implementation
    // }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::MoveFile(
    const string &sourceFilePath, const string &destFilePath,
    const shared_ptr<DirectoryTree> &dirTree, const shared_ptr<Cache> &cache) {
  assert(dirTree && cache);

  ClientError<QSError::Value> err = MoveObject(sourceFilePath, destFilePath);
  if (IsGoodQSError(err)) {
    if (dirTree && dirTree->Has(sourceFilePath)) {
      dirTree->Rename(sourceFilePath, destFilePath);
    }

    if (cache && cache->HasFile(sourceFilePath)) {
      cache->Rename(sourceFilePath, destFilePath);
    }
  } else {
    // Handle following special case
    // As for object storage, there is no concept of directory.
    // For some case, such as
    //   an object of "/abc/tst.txt" can exist without existing
    //   object of "/abc/"
    // In this case, putobject(move) with objKey of "/abc/" will not success.
    // So, we need to create it.
    string sourceFilePath1 = RTrim(sourceFilePath, ' ');
    bool isDir = sourceFilePath1[sourceFilePath1.size() - 1] == '/';
    if (err.GetError() == QSError::NOT_FOUND && isDir) {
      ClientError<QSError::Value> err1 = MakeDirectory(destFilePath);
      if (IsGoodQSError(err1)) {
        if (dirTree && dirTree->Has(sourceFilePath)) {
          dirTree->Rename(sourceFilePath, destFilePath);
        }
      } else {
        Info("Object not created : " + GetMessageForQSError(err1) +
             FormatPath(destFilePath));
      }
    }
  }

  return err;
}

// --------------------------------------------------------------------------
// Notes: MoveDirectory will do nothing on dir tree and cache.
ClientError<QSError::Value> QSClient::MoveDirectory(const string &sourceDirPath,
                                                    const string &targetDirPath,
                                                    bool async) {
  string sourceDir = AppendPathDelim(sourceDirPath);
  ListObjectsInput listObjInput;
  listObjInput.SetLimit(Constants::BucketListObjectsLimit);
  listObjInput.SetDelimiter(QS::Utils::GetPathDelimiter());
  string listprefix = IsRootDirectory(sourceDir)
                      ? string()
                      : AppendPathDelim(LTrim(sourceDir, '/'));
  listObjInput.SetPrefix(listprefix);
  // List the source directory all objects
  ListObjectsOutcome outcome = GetQSClientImpl()->ListObjects(
      &listObjInput, NULL, NULL, 0);

  if (!outcome.IsSuccess()) {
    DebugError("Fail to list objects " + FormatPath(sourceDir));
    return outcome.GetError();
  }

  string targetDir = AppendPathDelim(targetDirPath);
  size_t lenSourceDir = sourceDir.size();
  vector<ListObjectsOutput> &listObjOutputs = outcome.GetResult();
  PrintErrorMsg receivedHandler;

  // move sub files
  string prefix = LTrim(sourceDir, '/');
  BOOST_FOREACH (ListObjectsOutput &listObjOutput, listObjOutputs) {
    BOOST_FOREACH (const KeyType &key, listObjOutput.GetKeys()) {
      // sdk will put dir (if exists) itself into keys, ignore it
      if (prefix == const_cast<KeyType &>(key).GetKey()) {
        continue;
      }
      string sourceSubFile = "/" + const_cast<KeyType &>(key).GetKey();
      string targetSubFile = targetDir + sourceSubFile.substr(lenSourceDir);

      if (async) {  // asynchronously
        GetExecutor()->SubmitAsync(
            bind(boost::type<void>(), receivedHandler, _1),
            bind(boost::type<ClientError<QSError::Value> >(),
                 &QSClient::MoveObject, this, _1, _2),
            sourceSubFile, targetSubFile);
      } else {  // synchronizely
        receivedHandler(MoveObject(sourceSubFile, targetSubFile));
      }
    }
  }

  // move sub folders
  BOOST_FOREACH (ListObjectsOutput &listObjOutput, listObjOutputs) {
    BOOST_FOREACH (const string &commonPrefix,
                   listObjOutput.GetCommonPrefixes()) {
      string sourceSubDir = AppendPathDelim("/" + commonPrefix);
      string targetSubDir = targetDir + sourceSubDir.substr(lenSourceDir);

      if (async) {  // asynchronously
        GetExecutor()->SubmitAsync(
            bind(boost::type<void>(), receivedHandler, _1),
            bind(boost::type<ClientError<QSError::Value> >(),
                 &QSClient::MoveDirectory, this, _1, _2, false),
            sourceSubDir, targetSubDir);

      } else {  // synchronizely
        receivedHandler(MoveDirectory(sourceSubDir, targetSubDir));
      }
    }
  }

  // move dir itself
  if (async) {  // asynchronously
    GetExecutor()->SubmitAsync(bind(boost::type<void>(), receivedHandler, _1),
                               bind(boost::type<ClientError<QSError::Value> >(),
                                    &QSClient::MoveObject, this, _1, _2),
                               sourceDir, targetDir);
  } else {  // synchronizely
    receivedHandler(MoveObject(sourceDir, targetDir));
  }

  return ClientError<QSError::Value>(QSError::GOOD, false);
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::MoveObject(const string &sourcePath,
                                                 const string &targetPath) {
  PutObjectInput input;
  input.SetXQSMoveSource(BuildXQSSourceString(sourcePath));
  // sdk cpp require content-length parameter, though it will be ignored
  // so, set 0 to avoid sdk parameter checking failure
  input.SetContentLength(0);
  // it seems put-move discards the content-type, so we set directory
  // mime type explicitly
  string sourcePath1 = RTrim(sourcePath, ' ');
  bool isDir = sourcePath1[sourcePath1.size() - 1] == '/';
  if (isDir) {
    input.SetContentType(GetDirectoryMimeType());
  }

  PutObjectOutcome outcome = GetQSClientImpl()->PutObject(targetPath, &input);

  if (outcome.IsSuccess()) {
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::DownloadFile(const string &filePath,
                                                   shared_ptr<iostream> buffer,
                                                   const string &range,
                                                   string *eTag) {
  GetObjectInput input;
  if (!range.empty()) {
    input.SetRange(range);
  }

  GetObjectOutcome outcome = GetQSClientImpl()->GetObject(filePath, &input);

  if (outcome.IsSuccess()) {
    GetObjectOutput &res = outcome.GetResult();
    iostream *bodyStream = res.GetBody();
    bodyStream->seekg(0, std::ios_base::beg);
    buffer->seekp(0, std::ios_base::beg);
    (*buffer) << bodyStream->rdbuf();
    if (eTag != NULL) {
      *eTag = res.GetETag();
    }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::InitiateMultipartUpload(
    const string &filePath, string *uploadId) {
  InitiateMultipartUploadInput input;
  input.SetContentType(LookupMimeType(filePath));

  InitiateMultipartUploadOutcome outcome =
      GetQSClientImpl()->InitiateMultipartUpload(filePath, &input);

  if (outcome.IsSuccess()) {
    InitiateMultipartUploadOutput &res = outcome.GetResult();
    if (uploadId != NULL) {
      *uploadId = res.GetUploadID();
    }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::UploadMultipart(
    const string &filePath, const string &uploadId, int partNumber,
    uint64_t contentLength, shared_ptr<iostream> buffer) {
  UploadMultipartInput input;
  input.SetUploadID(uploadId);
  input.SetPartNumber(partNumber);
  input.SetContentLength(contentLength);
  if (contentLength > 0) {
    input.SetBody(buffer.get());
  }

  UploadMultipartOutcome outcome =
      GetQSClientImpl()->UploadMultipart(filePath, &input);

  if (outcome.IsSuccess()) {
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::CompleteMultipartUpload(
    const string &filePath, const string &uploadId,
    const vector<int> &sortedPartIds) {
  CompleteMultipartUploadInput input;
  input.SetUploadID(uploadId);
  vector<ObjectPartType> objParts;
  BOOST_FOREACH (int partId, sortedPartIds) {
    ObjectPartType part;
    part.SetPartNumber(partId);
    objParts.push_back(part);
  }
  input.SetObjectParts(objParts);

  CompleteMultipartUploadOutcome outcome =
      GetQSClientImpl()->CompleteMultipartUpload(filePath, &input);

  if (outcome.IsSuccess()) {
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::AbortMultipartUpload(
    const string &filePath, const string &uploadId) {
  AbortMultipartUploadInput input;
  input.SetUploadID(uploadId);

  AbortMultipartUploadOutcome outcome =
      GetQSClientImpl()->AbortMultipartUpload(filePath, &input);

  if (outcome.IsSuccess()) {
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::UploadFile(const string &filePath,
                                                 uint64_t fileSize,
                                                 shared_ptr<iostream> buffer) {
  PutObjectInput input;
  input.SetContentLength(fileSize);
  input.SetContentType(LookupMimeType(filePath));
  if (fileSize > 0) {
    input.SetBody(buffer.get());
  }

  PutObjectOutcome outcome = GetQSClientImpl()->PutObject(filePath, &input);

  if (outcome.IsSuccess()) {
    // As sdk doesn't return the created file meta data in PutObjectOutput,
    // So we cannot grow the directory tree here, instead we need to call
    // Stat to head the object again in Drive::MakeFile;
    //
    // auto &drive = Drive::Instance();
    // auto &dirTree = Drive::Instance().GetDirectoryTree();
    // if (dirTree) {
    //   dirTree->Grow(PutObjectOutputToFileMeta());  // no implementation
    // }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::SymLink(const string &filePath,
                                              const string &linkPath) {
  PutObjectInput input;
  input.SetContentLength(filePath.size());
  input.SetContentType(GetSymlinkMimeType());
  shared_ptr<stringstream> ss = make_shared<stringstream>(filePath);
  input.SetBody(ss.get());

  PutObjectOutcome outcome = GetQSClientImpl()->PutObject(linkPath, &input);

  if (outcome.IsSuccess()) {
    // As sdk doesn't return the created file meta data in PutObjectOutput,
    // So we cannot grow the directory tree here, instead we need to call
    // Stat to head the object again in Drive::MakeFile;
    //
    // auto &drive = Drive::Instance();
    // auto &dirTree = Drive::Instance().GetDirectoryTree();
    // if (dirTree) {
    //   dirTree->Grow(PutObjectOutputToFileMeta());  // no implementation
    // }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::ListDirectory(
    const string &dirPath, const shared_ptr<DirectoryTree> &dirTree) {
  assert(dirTree);
  uint64_t maxListCount = ClientConfiguration::Instance().GetMaxListCount();
  bool listAll = maxListCount <= 0;

  // Set maxCount for a single list operation.
  // This will request for ListObjects seperately, so we can construct
  // directory tree gradually. This will be helpful for the performance
  // if there are a huge number of objects to list.
  uint64_t maxCountPerList =
      static_cast<uint64_t>(Constants::BucketListObjectsLimit * 2);
  if (!listAll && maxListCount < maxCountPerList) {
    maxCountPerList = maxListCount;
  }

  shared_ptr<Node> dirNode = dirTree->Find(dirPath);
  vector<shared_ptr<FileMetaData> > allFileMetaDatas;

  ListObjectsInput listObjInput;
  uint64_t limit = Constants::BucketListObjectsLimit;
  if (!listAll  && maxCountPerList < limit) {
    // maxCount ==0 means list all objects
    limit = maxCountPerList;
  }
  listObjInput.SetLimit(limit);
  listObjInput.SetDelimiter(QS::Utils::GetPathDelimiter());
  string prefix = IsRootDirectory(dirPath)
                      ? string()
                      : AppendPathDelim(LTrim(dirPath, '/'));
  listObjInput.SetPrefix(prefix);

  bool resultTruncated = false;
  uint64_t resCount = 0;

  do {
    uint64_t countListed = 0;
    ListObjectsOutcome outcome = GetQSClientImpl()->ListObjects(
        &listObjInput, &resultTruncated, &countListed, maxCountPerList);

    if (!outcome.IsSuccess()) {
      return outcome.GetError();
    }

    resCount += countListed;
    BOOST_FOREACH (ListObjectsOutput &listObjOutput, outcome.GetResult()) {
      vector<shared_ptr<FileMetaData> > fileMetaDatas;
      if (!(dirNode && *dirNode)) {  // directory not existing
        // Add its children to dir tree
        fileMetaDatas = QSClientConverter::ListObjectsOutputToFileMetaDatas(
            listObjOutput, true);  // add dir itself
      } else {                     // directory existing
        fileMetaDatas = QSClientConverter::ListObjectsOutputToFileMetaDatas(
            listObjOutput, false);  // not add dir itself
      }
      allFileMetaDatas.insert(allFileMetaDatas.end(), fileMetaDatas.begin(),
                              fileMetaDatas.end());
    }  // for list object output
  } while (resultTruncated && (listAll || resCount < maxListCount));

  if (!(dirNode && *dirNode)) {  // directory not existing
    dirTree->Grow(allFileMetaDatas);
  } else {  // directory existing
    if (dirNode->IsEmpty()) {
      dirTree->Grow(allFileMetaDatas);
    } else {
      dirTree->UpdateDirectory(dirPath, allFileMetaDatas);
    }
  }

  return ClientError<QSError::Value>(QSError::GOOD, false);
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::Stat(
    const string &path, const shared_ptr<DirectoryTree> &dirTree,
    time_t modifiedSince, bool *modified) {
  assert(dirTree);

  if (modified != NULL) {
    *modified = false;
  }

  if (IsRootDirectory(path)) {
    // Stat aims to get object meta data. As in object storage, bucket has no
    // data to record last modified time.
    // Bucket mtime is set when connect to it at the first time.
    // We just think bucket mtime is not modified since then.
    return ClientError<QSError::Value>(QSError::GOOD, false);
  }

  HeadObjectInput input;
  if (modifiedSince > 0) {
    input.SetIfModifiedSince(SecondsToRFC822GMT(modifiedSince));
  }

  HeadObjectOutcome outcome = GetQSClientImpl()->HeadObject(path, &input);

  if (outcome.IsSuccess()) {
    HeadObjectOutput &res = outcome.GetResult();
    if (res.GetResponseCode() == QingStor::Http::NOT_MODIFIED) {
      // if is not modified, no meta is returned, so just return directly
      return ClientError<QSError::Value>(QSError::GOOD, false);
    }

    if (modified != NULL) {
      *modified = true;
    }
    shared_ptr<FileMetaData> fileMetaData =
        QSClientConverter::HeadObjectOutputToFileMetaData(path, res);
    if (fileMetaData) {
      dirTree->Grow(fileMetaData);  // add/update node in dir tree
    }
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    // Handle following special case.
    // As for object storage, there is no concept of directory.
    // For some case, such as
    //   an object of "/abc/tst.txt" can exist without existing
    //   object of "/abc/"
    // In this case, headobject with objKey of "/abc/" will not success.
    // So, we need to use listobject with prefix of "/abc/" to confirm if a
    // directory node is actually needed to be construct in dir tree.
    ClientError<QSError::Value> err = outcome.GetError();
    if (err.GetError() == QSError::NOT_FOUND) {
      if (path[path.size() - 1] == '/') {
        ListObjectsInput listObjInput;
        listObjInput.SetLimit(2);
        listObjInput.SetDelimiter(QS::Utils::GetPathDelimiter());
        listObjInput.SetPrefix(LTrim(path, '/'));
        ListObjectsOutcome outcome =
            GetQSClientImpl()->ListObjects(&listObjInput, NULL, NULL, 10);

        if (outcome.IsSuccess()) {
          bool dirExist = false;
          BOOST_FOREACH (ListObjectsOutput &listObjOutput,
                         outcome.GetResult()) {
            if (!listObjOutput.GetKeys().empty() ||
                !listObjOutput.GetCommonPrefixes().empty()) {
              dirExist = true;
              break;
            }
          }

          if (dirExist) {
            if (modified != NULL) {
              *modified = true;
            }
            dirTree->Grow(BuildDefaultDirectoryMeta(path));  // add dir node
            return ClientError<QSError::Value>(QSError::GOOD, false);
          }  // if dir exist
        }    // if outcome is success
      }      // if path is dir
    }

    return err;
  }
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSClient::Statvfs(struct statvfs *stvfs) {
  assert(stvfs != NULL);
  if (stvfs == NULL) {
    DebugError("Null statvfs parameter");
    return ClientError<QSError::Value>(QSError::PARAMETER_MISSING, false);
  }

  GetBucketStatisticsOutcome outcome = GetQSClientImpl()->GetBucketStatistics();

  if (outcome.IsSuccess()) {
    QingStor::GetBucketStatisticsOutput &res = outcome.GetResult();
    QSClientConverter::GetBucketStatisticsOutputToStatvfs(res, stvfs);
    return ClientError<QSError::Value>(QSError::GOOD, false);
  } else {
    return outcome.GetError();
  }
}

// --------------------------------------------------------------------------
const shared_ptr<QsConfig> &QSClient::GetQingStorConfig() {
  StartQSService();
  return m_qingStorConfig;
}

// --------------------------------------------------------------------------
const shared_ptr<QSClientImpl> &QSClient::GetQSClientImpl() const {
  return const_cast<QSClient *>(this)->GetQSClientImpl();
}

// --------------------------------------------------------------------------
shared_ptr<QSClientImpl> &QSClient::GetQSClientImpl() {
  call_once(onceFlagGetClientImpl,
            bind(boost::type<void>(), &QSClient::DoGetQSClientImpl, this));
  return m_qsClientImpl;
}

// --------------------------------------------------------------------------
void QSClient::DoGetQSClientImpl() {
  shared_ptr<ClientImpl> r = GetClientImpl();
  assert(r);
  FatalIf(!r, "QSClient is initialized with null QSClientImpl");
  QSClientImpl *p = dynamic_cast<QSClientImpl *>(r.get());
  m_qsClientImpl = shared_ptr<QSClientImpl>(r, p);
}

// --------------------------------------------------------------------------
void QSClient::StartQSService() {
  call_once(onceFlagStartService, QSClient::DoStartQSService);
}

// --------------------------------------------------------------------------
void QSClient::DoStartQSService() {
  const ClientConfiguration &clientConfig = ClientConfiguration::Instance();
  // sdk log level
  LogLevel sdkLogLevel;
  if (clientConfig.GetClientLogLevel() == ClientLogLevel::Verbose) {
    sdkLogLevel = Verbose;
  } else if (clientConfig.GetClientLogLevel() == ClientLogLevel::Debug) {
    sdkLogLevel = Debug;
  } else if (clientConfig.GetClientLogLevel() == ClientLogLevel::Info) {
    sdkLogLevel = Info;
  } else if (clientConfig.GetClientLogLevel() == ClientLogLevel::Warn) {
    sdkLogLevel = Warning;
  } else if (clientConfig.GetClientLogLevel() == ClientLogLevel::Error) {
    sdkLogLevel = Error;
  } else if (clientConfig.GetClientLogLevel() == ClientLogLevel::Fatal) {
    sdkLogLevel = Fatal;
  } else {
    sdkLogLevel = Warning;  // default
  }
  m_sdkOptions.logLevel = sdkLogLevel;
  m_sdkOptions.logPath = GetSDKLogDir();
  InitializeSDK(m_sdkOptions);

  // sdk config
  m_qingStorConfig = shared_ptr<QsConfig>(
      new QsConfig(clientConfig.GetAccessKeyId(), clientConfig.GetSecretKey()));

  m_qingStorConfig->additionalUserAgent = clientConfig.GetAdditionalAgent();
  m_qingStorConfig->host = Http::HostToString(clientConfig.GetHost());
  m_qingStorConfig->protocol =
      Http::ProtocolToString(clientConfig.GetProtocol());
  m_qingStorConfig->port = clientConfig.GetPort();
  m_qingStorConfig->connectionRetries = clientConfig.GetTransactionRetries();
  // timeoutPeriod is for one connection duration
  m_qingStorConfig->timeOutPeriod = clientConfig.GetTransactionTimeDuration();
  // QSClient has not count on the retry strategy, instead QSClient count on
  // qingstor sdk retry policy
}

// --------------------------------------------------------------------------
void QSClient::CloseQSService() { ShutdownSDK(m_sdkOptions); }

// --------------------------------------------------------------------------
void QSClient::InitializeClientImpl() {
  const ClientConfiguration &clientConfig = ClientConfiguration::Instance();
  if (GetQSClientImpl()->GetBucket()) return;
  GetQSClientImpl()->SetBucket(shared_ptr<Bucket>(new Bucket(
      *m_qingStorConfig, clientConfig.GetBucket(), clientConfig.GetZone())));
}

shared_ptr<QingStor::QsConfig> QSClient::m_qingStorConfig;
QingStor::SDKOptions QSClient::m_sdkOptions;

}  // namespace Client
}  // namespace QS
