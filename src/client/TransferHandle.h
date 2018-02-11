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
#ifndef QSFS_CLIENT_TRANSFERHANDLE_H_
#define QSFS_CLIENT_TRANSFERHANDLE_H_

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint16_t uint64_t

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"

#include "client/QSError.h"

namespace QS {

namespace Client {

class TransferManager;
class TransferHandle;
class Part;
class QSTransferManager;

typedef std::map<uint16_t, boost::shared_ptr<Part> > PartIdToPartMap;
typedef PartIdToPartMap::iterator PartIdToPartMapIterator;

class Part {
 public:
  Part(uint16_t partId = 0, size_t bestProgressInBytes = 0,
       size_t sizeInBytes = 0, size_t rangeBegin = 0);

  ~Part() {}

 public:
  std::string ToString() const;
  // accessor
  uint16_t GetPartId() const { return m_partId; }
  const std::string &GetETag() const { return m_eTag; }
  size_t GetBestProgress() const { return m_bestProgress; }
  size_t GetSize() const { return m_size; }
  size_t GetRangeBegin() const { return m_rangeBegin; }

  boost::shared_ptr<std::iostream> GetDownloadPartStream() const {
    return boost::atomic_load(&m_downloadPartStream);
  }

 private:
  void Reset() { m_currentProgress = 0; }
  void OnDataTransferred(uint64_t amount,
                         const boost::shared_ptr<TransferHandle> &handle);
  // mutator
  void SetETag(const std::string &etag) { m_eTag = etag; }
  void SetBestProgress(size_t bestProgressInBytes) {
    m_bestProgress = bestProgressInBytes;
  }
  void SetSize(size_t sizeInBytes) { m_size = sizeInBytes; }
  void SetRangeBegin(size_t rangeBegin) { m_rangeBegin = rangeBegin; }

  void SetDownloadPartStream(
      boost::shared_ptr<std::iostream> downloadPartStream) {
    boost::atomic_store(&m_downloadPartStream, downloadPartStream);
  }

 private:
  uint16_t m_partId;
  std::string m_eTag;        // could be empty
  size_t m_currentProgress;  // in bytes
  size_t m_bestProgress;     // in bytes
  size_t m_size;             // in bytes
  size_t m_rangeBegin;

  // Notice: use atomic functions every time you touch the variable
  boost::shared_ptr<std::iostream> m_downloadPartStream;

  friend class TransferHandle;
  friend class QSTransferManager;
  friend struct ReceivedHandlerSingleDownload;
  friend struct ReceivedHandlerMultipleDownload;
  friend struct ReceivedHandlerSingleUpload;
  friend struct ReceivedHandlerMultipleUpload;
};

struct TransferStatus {
  enum Value {
    NotStarted,  // operation is still queued and has not been processing
    InProgress,  // operation is now running
    Cancelled,   // operation is cancelled, can still be retried
    Failed,      // operation failed, can still be retried
    Completed,   // operation was sucessful
    Aborted      // operation either failed or cancelled and a user deleted the
                 // multi-part upload
  };
};

struct TransferDirection {
  enum Value { Upload, Download };
};


class TransferHandle : private boost::noncopyable {
 public:
  // Ctor
  TransferHandle(const std::string &bucket, const std::string &objKey,
                 size_t contentRangeBegin, uint64_t totalTransferSize,
                 TransferDirection::Value direction,
                 const std::string &targetFilePath = std::string());

  ~TransferHandle() { ReleaseDownloadStream(); }

 public:
  bool IsMultipart() const { return m_isMultipart; }
  const std::string &GetMultiPartId() const { return m_multipartId; }
  PartIdToPartMap GetQueuedParts() const;
  PartIdToPartMap GetPendingParts() const;
  PartIdToPartMap GetFailedParts() const;
  PartIdToPartMap GetCompletedParts() const;
  bool HasQueuedParts() const;
  bool HasPendingParts() const;
  bool HasFailedParts() const;
  bool HasParts() const;

  // Notes the transfer progress 's two invariants
  uint64_t GetBytesTransferred() const {
    boost::lock_guard<boost::mutex> locker(m_bytesTransferredLock);
    return m_bytesTransferred;
  }
  uint64_t GetBytesTotalSize() const {
    boost::lock_guard<boost::mutex> locker(m_bytesTotalSizeLock);
    return m_bytesTotalSize;
  }
  TransferDirection::Value GetDirection() const { return m_direction; }
  bool ShouldContinue() const {
    boost::lock_guard<boost::mutex> locker(m_cancelLock);
    return !m_cancel;
  }
  TransferStatus::Value GetStatus() const {
    boost::lock_guard<boost::mutex> locker(m_statusLock);
    return m_status;
  }

  const std::string &GetTargetFilePath() const { return m_targetFilePath; }

  const std::string &GetBucket() const { return m_bucket; }
  const std::string &GetObjectKey() const { return m_objectKey; }
  size_t GetContentRangeBegin() const { return m_contentRangeBegin; }
  const std::string &GetContentType() const { return m_contentType; }
  const std::map<std::string, std::string> &GetMetadata() const {
    return m_metadata;
  }

  const ClientError<QSError::Value> &GetError() const { return m_error; }

 public:
  void WaitUntilFinished() const;
  bool DoneTransfer() const;

 private:
  void SetIsMultiPart(bool isMultipart) { m_isMultipart = isMultipart; }
  void SetMultipartId(const std::string &multipartId) {
    m_multipartId = multipartId;
  }
  void AddQueuePart(const boost::shared_ptr<Part> &part);
  void AddPendingPart(const boost::shared_ptr<Part> &part);
  void ChangePartToFailed(const boost::shared_ptr<Part> &part);
  void ChangePartToCompleted(const boost::shared_ptr<Part> &part,
                             const std::string &eTag = std::string());
  void UpdateBytesTransferred(uint64_t amount) {
    boost::lock_guard<boost::mutex> locker(m_bytesTransferredLock);
    m_bytesTransferred += amount;
  }
  void SetBytesTotalSize(uint64_t totalSize) {
    boost::lock_guard<boost::mutex> locker(m_bytesTotalSizeLock);
    m_bytesTotalSize = totalSize;
  }

  // Cancel transfer, this happens asynchronously, if you need to wait for it to
  // be cancelled, either handle the callbacks or call WaitUntilFinished
  void Cancle() {
    boost::lock_guard<boost::mutex> locker(m_cancelLock);
    m_cancel = true;
  }
  // Reset the cancellation for a retry. This will be done automatically by
  // transfer manager
  void Restart() {
    boost::lock_guard<boost::mutex> locker(m_cancelLock);
    m_cancel = false;
  }
  void UpdateStatus(TransferStatus::Value status);

  void WritePartToDownloadStream(
      const boost::shared_ptr<std::iostream> &partStream, size_t offset);

  void SetDownloadStream(boost::shared_ptr<std::iostream> downloadStream) {
    boost::atomic_store(&m_downloadStream, downloadStream);
  }
  boost::shared_ptr<std::iostream> GetDownloadStream() const {
    return boost::atomic_load(&m_downloadStream);
  }

  void ReleaseDownloadStream();
  void SetTargetFilePath(const std::string &path) { m_targetFilePath = path; }

  void SetBucket(const std::string &bucket) { m_bucket = bucket; }
  void SetObjectKey(const std::string &key) { m_objectKey = key; }
  void SetContentRangeBegin(size_t rangeBegin) {
    m_contentRangeBegin = rangeBegin;
  }
  void SetContentType(const std::string &contentType) {
    m_contentType = contentType;
  }
  void SetMetadata(const std::map<std::string, std::string> &metadata) {
    m_metadata = metadata;
  }

  void SetError(const ClientError<QSError::Value> &error) { m_error = error; }

  // Internal use only
  bool Predicate() const;

 private:
  TransferHandle() {}

  bool m_isMultipart;
  std::string m_multipartId;  // mulitpart upload id
  PartIdToPartMap m_queuedParts;
  PartIdToPartMap m_pendingParts;
  PartIdToPartMap m_failedParts;
  PartIdToPartMap m_completedParts;
  mutable boost::mutex m_partsLock;

  mutable boost::mutex m_bytesTransferredLock;
  uint64_t m_bytesTransferred;  // size have been transferred

  mutable boost::mutex m_bytesTotalSizeLock;
  uint64_t m_bytesTotalSize;    // the total size need to be transferred

  TransferDirection::Value m_direction;

  mutable boost::mutex m_cancelLock;
  bool m_cancel;

  mutable boost::mutex m_statusLock;
  TransferStatus::Value m_status;

  mutable boost::condition_variable m_waitUntilFinishSignal;

  mutable boost::recursive_mutex m_downloadStreamLock;
  boost::shared_ptr<std::iostream> m_downloadStream;
  // If known, this is the location of the local file being uploaded from,
  // or downloaded to.
  // If use stream API, this will always be blank.
  std::string m_targetFilePath;

  std::string m_bucket;
  std::string m_objectKey;
  size_t m_contentRangeBegin;
  // content type of object being transferred
  std::string m_contentType;
  // In case of an upload, this is the metadata that was placed on the object.
  // In case of a download, this is the object metadata from the GET operation.
  std::map<std::string, std::string> m_metadata;

  ClientError<QSError::Value> m_error;

  friend class QSTransferManager;
  friend class Part;
  friend struct ReceivedHandlerSingleDownload;
  friend struct ReceivedHandlerMultipleDownload;
  friend struct ReceivedHandlerSingleUpload;
  friend struct ReceivedHandlerMultipleUpload;
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_TRANSFERHANDLE_H_
