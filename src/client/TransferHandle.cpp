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

#include "client/TransferHandle.h"

#include <assert.h>

#include <string>
#include <utility>

#include "boost/bind.hpp"
#include "boost/exception/to_string.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"

#include "base/LogMacros.h"

namespace QS {

namespace Client {

using boost::bind;
using boost::lock_guard;
using boost::mutex;
using boost::recursive_mutex;
using boost::shared_ptr;
using boost::to_string;
using boost::unique_lock;
using std::iostream;
using std::make_pair;
using std::pair;
using std::string;

namespace {
bool IsFinishedStatus(TransferStatus::Value status) {
  return !(status == TransferStatus::NotStarted ||
           status == TransferStatus::InProgress);
}

bool AllowTransition(TransferStatus::Value current,
                     TransferStatus::Value next) {
  if (IsFinishedStatus(current) && IsFinishedStatus(next)) {
    return current == TransferStatus::Cancelled &&
           next == TransferStatus::Aborted;
  }
  return true;
}

}  // namespace

// --------------------------------------------------------------------------
Part::Part(uint16_t partId, size_t bestProgressInBytes, size_t sizeInBytes,
           size_t rangeBegin)
    : m_partId(partId),
      m_eTag(""),
      m_currentProgress(0),
      m_bestProgress(bestProgressInBytes),
      m_size(sizeInBytes),
      m_rangeBegin(rangeBegin),
      m_downloadPartStream() {}

// --------------------------------------------------------------------------
string Part::ToString() const {
  return "[part id: " + to_string(m_partId) + ", etag: " + m_eTag +
         ", current progress(bytes): " + to_string(m_currentProgress) +
         ", best progress(bytes): " + to_string(m_bestProgress) +
         ", size(bytes): " + to_string(m_size) +
         ", range begin: " + to_string(m_rangeBegin) + "]";
}

// --------------------------------------------------------------------------
void Part::OnDataTransferred(uint64_t amount,
                             const shared_ptr<TransferHandle> &handle) {
  m_currentProgress += static_cast<size_t>(amount);
  if (m_currentProgress > m_bestProgress) {
    handle->UpdateBytesTransferred(m_currentProgress - m_bestProgress);
    m_bestProgress = m_currentProgress;
  }
}

// --------------------------------------------------------------------------
TransferHandle::TransferHandle(const string &bucket, const string &objKey,
                               size_t contentRangeBegin,
                               uint64_t totalTransferSize,
                               TransferDirection::Value direction,
                               const string &targetFilePath)
    : m_isMultipart(false),
      m_multipartId(),
      m_bytesTransferred(0),
      m_bytesTotalSize(totalTransferSize),
      m_direction(direction),
      m_cancel(false),
      m_status(TransferStatus::NotStarted),
      m_downloadStream(),
      m_targetFilePath(targetFilePath),
      m_bucket(bucket),
      m_objectKey(objKey),
      m_contentRangeBegin(contentRangeBegin),
      m_contentType() {}

// --------------------------------------------------------------------------
PartIdToPartMap TransferHandle::GetQueuedParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return m_queuedParts;
}

// --------------------------------------------------------------------------
PartIdToPartMap TransferHandle::GetPendingParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return m_pendingParts;
}

// --------------------------------------------------------------------------
PartIdToPartMap TransferHandle::GetFailedParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return m_failedParts;
}

// --------------------------------------------------------------------------
PartIdToPartMap TransferHandle::GetCompletedParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return m_completedParts;
}

// --------------------------------------------------------------------------
bool TransferHandle::HasQueuedParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return !m_queuedParts.empty();
}

// --------------------------------------------------------------------------
bool TransferHandle::HasPendingParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return !m_pendingParts.empty();
}

// --------------------------------------------------------------------------
bool TransferHandle::HasFailedParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return !m_failedParts.empty();
}

// --------------------------------------------------------------------------
bool TransferHandle::HasParts() const {
  lock_guard<mutex> lock(m_partsLock);
  return !m_failedParts.empty() || !m_queuedParts.empty() ||
         !m_pendingParts.empty();
}

// --------------------------------------------------------------------------
bool TransferHandle::DoneTransfer() const {
  lock_guard<mutex> lock(m_bytesTransferredLock);
  return m_bytesTransferred == m_bytesTotalSize;
}
// --------------------------------------------------------------------------
void TransferHandle::AddQueuePart(const shared_ptr<Part> &part) {
  lock_guard<mutex> lock(m_partsLock);
  part->Reset();
  m_failedParts.erase(part->GetPartId());
  pair<PartIdToPartMapIterator, bool> res =
      m_queuedParts.insert(make_pair(part->GetPartId(), part));
  if (!res.second) {
    DebugWarning("Fail to add to queue parts with part " + part->ToString());
  }
}

// --------------------------------------------------------------------------
void TransferHandle::AddPendingPart(const shared_ptr<Part> &part) {
  lock_guard<mutex> lock(m_partsLock);
  m_queuedParts.erase(part->GetPartId());
  pair<PartIdToPartMapIterator, bool> res =
      m_pendingParts.insert(make_pair(part->GetPartId(), part));
  if (!res.second) {
    DebugWarning("Fail to add to pending parts with part " + part->ToString());
  }
}

// --------------------------------------------------------------------------
void TransferHandle::ChangePartToFailed(const shared_ptr<Part> &part) {
  uint16_t partId = part->GetPartId();
  lock_guard<mutex> lock(m_partsLock);
  part->Reset();
  m_queuedParts.erase(partId);
  m_pendingParts.erase(partId);
  pair<PartIdToPartMapIterator, bool> res =
      m_failedParts.insert(make_pair(partId, part));
  if (!res.second) {
    DebugWarning("Fail to change part state to failed with part " +
                 part->ToString());
  }
}

// --------------------------------------------------------------------------
void TransferHandle::ChangePartToCompleted(const shared_ptr<Part> &part,
                                           const string &eTag) {
  uint16_t partId = part->GetPartId();
  lock_guard<mutex> lock(m_partsLock);
  if (m_pendingParts.erase(partId) == 0) {
    m_failedParts.erase(partId);
  }
  if (!eTag.empty()) {
    part->SetETag(eTag);
  }
  pair<PartIdToPartMapIterator, bool> res =
      m_completedParts.insert(make_pair(partId, part));
  if (!res.second) {
    DebugWarning("Fail to change part state to completed with part " +
                 part->ToString());
  }
}

// --------------------------------------------------------------------------
void TransferHandle::UpdateStatus(TransferStatus::Value newStatus) {
  unique_lock<mutex> lock(m_statusLock);
  if (AllowTransition(m_status, newStatus)) {
    m_status = newStatus;
    if (IsFinishedStatus(newStatus)) {
      if (newStatus == TransferStatus::Completed) {
        ReleaseDownloadStream();
      }
      lock.unlock();
      m_waitUntilFinishSignal.notify_all();
    }
  }
}

// --------------------------------------------------------------------------
void TransferHandle::WaitUntilFinished() const {
  unique_lock<mutex> lock(m_statusLock);
  m_waitUntilFinishSignal.wait(
      lock, bind(boost::type<bool>(), &TransferHandle::Predicate, this));
}

// --------------------------------------------------------------------------
void TransferHandle::WritePartToDownloadStream(
    const shared_ptr<iostream> &partStream, size_t offset) {
  lock_guard<recursive_mutex> lock(m_downloadStreamLock);
  assert(m_downloadStream && partStream);
  if (!m_downloadStream) {
    DebugError("Try to write part to a null download stream");
    return;
  }
  if (!partStream) {
    DebugError("Try to write part to download stream with a null stream input");
    return;
  }

  partStream->seekg(0, std::ios_base::beg);
  m_downloadStream->seekp(offset);
  (*m_downloadStream) << partStream->rdbuf();
  m_downloadStream->flush();
}

// --------------------------------------------------------------------------
void TransferHandle::ReleaseDownloadStream() {
  lock_guard<recursive_mutex> lock(m_downloadStreamLock);
  if (m_downloadStream) {
    m_downloadStream->flush();
    m_downloadStream.reset();
  }
}

// --------------------------------------------------------------------------
bool TransferHandle::Predicate() const {
  return IsFinishedStatus(m_status) && !HasPendingParts();
}

}  // namespace Client
}  // namespace QS
