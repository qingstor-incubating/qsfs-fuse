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

#include "client/QSTransferManager.h"

#include <assert.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "boost/bind.hpp"
#include "boost/exception/to_string.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/tuple/tuple.hpp"

#include "base/LogMacros.h"
#include "client/Client.h"
#include "client/ClientConfiguration.h"
#include "client/QSError.h"
#include "client/TransferHandle.h"
#include "client/Utils.h"
#include "configure/Default.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/IOStream.h"
#include "data/Node.h"
#include "data/ResourceManager.h"
#include "data/StreamBuf.h"

namespace QS {

namespace Client {

using boost::bind;
using boost::make_shared;
using boost::shared_ptr;
using boost::to_string;
using QS::Client::Utils::BuildRequestRange;
using QS::Data::Buffer;
using QS::Data::Cache;
using QS::Data::ContentRangeDeque;
using QS::Data::DirectoryTree;
using QS::Data::IOStream;
using QS::Data::Node;
using QS::Data::ResourceManager;
using QS::Data::StreamBuf;
using QS::Configure::Default::GetUploadMultipartMinPartSize;
using QS::Configure::Default::GetUploadMultipartThresholdSize;
using std::iostream;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

// --------------------------------------------------------------------------
struct ReceivedHandlerSingleDownload {
  shared_ptr<TransferHandle> handle;
  shared_ptr<Part> part;

  ReceivedHandlerSingleDownload(const shared_ptr<TransferHandle> &handle_,
                                const shared_ptr<Part> &part_)
      : handle(handle_), part(part_) {}

  void operator()(const pair<ClientError<QSError::Value>, string> &outcome) {
    const ClientError<QSError::Value> &err = outcome.first;
    const string &eTag = outcome.second;
    if (IsGoodQSError(err)) {
      part->OnDataTransferred(part->GetSize(), handle);
      handle->ChangePartToCompleted(part, eTag);
      handle->UpdateStatus(TransferStatus::Completed);
    } else {
      handle->ChangePartToFailed(part);
      handle->UpdateStatus(TransferStatus::Failed);
      handle->SetError(err);
      Error(GetMessageForQSError(err));
    }
  }
};

// --------------------------------------------------------------------------
struct ReceivedHandlerMultipleDownload {
  shared_ptr<TransferHandle> handle;
  shared_ptr<Part> part;
  shared_ptr<ResourceManager> bufferManager;

  ReceivedHandlerMultipleDownload(const shared_ptr<TransferHandle> &handle_,
                                  const shared_ptr<Part> &part_,
                                  const shared_ptr<ResourceManager> &manager_)
      : handle(handle_), part(part_), bufferManager(manager_) {}

  void operator()(const pair<ClientError<QSError::Value>, string> &outcome) {
    const ClientError<QSError::Value> &err = outcome.first;
    const string &eTag = outcome.second;
    // write part stream to download stream
    if (IsGoodQSError(err)) {
      if (handle->ShouldContinue()) {
        handle->WritePartToDownloadStream(part->GetDownloadPartStream(),
                                          part->GetRangeBegin());
        part->OnDataTransferred(part->GetSize(), handle);
        handle->ChangePartToCompleted(part, eTag);
      } else {
        handle->ChangePartToFailed(part);
      }
    } else {
      handle->ChangePartToFailed(part);
      handle->SetError(err);
      Error(GetMessageForQSError(err));
    }

    // release part buffer back to resource manager
    if (part->GetDownloadPartStream()) {
      part->GetDownloadPartStream()->seekg(0, std::ios_base::beg);
      StreamBuf *partStreamBuf =
          dynamic_cast<StreamBuf *>(part->GetDownloadPartStream()->rdbuf());
      if (partStreamBuf) {
        bufferManager->Release(partStreamBuf->ReleaseBuffer());
        part->SetDownloadPartStream(shared_ptr<iostream>());
      }
    }

    // update status
    if (!handle->HasPendingParts() && !handle->HasQueuedParts()) {
      if (!handle->HasFailedParts() && handle->DoneTransfer()) {
        handle->UpdateStatus(TransferStatus::Completed);
      } else {
        handle->UpdateStatus(TransferStatus::Failed);
      }
    }
  }
};

// --------------------------------------------------------------------------
struct ReceivedHandlerSingleUpload {
  shared_ptr<TransferHandle> handle;
  shared_ptr<Part> part;
  shared_ptr<IOStream> stream;

  ReceivedHandlerSingleUpload(const shared_ptr<TransferHandle> &handle_,
                              const shared_ptr<Part> &part_,
                              const shared_ptr<IOStream> &stream_)
      : handle(handle_), part(part_), stream(stream_) {}

  void operator()(const ClientError<QSError::Value> &err) {
    if (IsGoodQSError(err)) {
      part->OnDataTransferred(handle->GetBytesTotalSize(), handle);
      handle->ChangePartToCompleted(
          part);  // without eTag, as sdk PutObjectOutput not return etag
      handle->UpdateStatus(TransferStatus::Completed);
      stream->seekg(0, std::ios_base::beg);
      StreamBuf *streamBuf = dynamic_cast<StreamBuf *>(stream->rdbuf());
      if (streamBuf) {
        Buffer buf = streamBuf->ReleaseBuffer();
        if (buf) {
          buf.reset();
        }
      }
    } else {
      handle->ChangePartToFailed(part);
      handle->UpdateStatus(TransferStatus::Failed);
      handle->SetError(err);
      Error(GetMessageForQSError(err));
    }
  }
};

// --------------------------------------------------------------------------
struct ReceivedHandlerMultipleUpload {
  shared_ptr<TransferHandle> handle;
  shared_ptr<Part> part;
  shared_ptr<IOStream> stream;
  shared_ptr<ResourceManager> bufferManager;
  shared_ptr<Client> client;

  ReceivedHandlerMultipleUpload(const shared_ptr<TransferHandle> &handle_,
                                const shared_ptr<Part> &part_,
                                const shared_ptr<IOStream> &stream_,
                                const shared_ptr<ResourceManager> &manager_,
                                const shared_ptr<Client> &client_)
      : handle(handle_),
        part(part_),
        stream(stream_),
        bufferManager(manager_),
        client(client_) {}

  void operator()(const ClientError<QSError::Value> &err) {
    if (IsGoodQSError(err)) {
      part->OnDataTransferred(part->GetSize(), handle);
      handle->ChangePartToCompleted(part);
    } else {
      handle->ChangePartToFailed(part);
      handle->SetError(err);
      Error(GetMessageForQSError(err));
    }

    // release part buffer back to resouce manager
    stream->seekg(0, std::ios_base::beg);
    StreamBuf *partStreamBuf = dynamic_cast<StreamBuf *>(stream->rdbuf());
    if (partStreamBuf) {
      bufferManager->Release(partStreamBuf->ReleaseBuffer());
    }

    // update status
    if (!handle->HasPendingParts() && !handle->HasQueuedParts()) {
      if (!handle->HasFailedParts() && handle->DoneTransfer()) {
        // complete multipart upload
        std::set<int> partIds;
        BOOST_FOREACH(const PartIdToPartMapIterator::value_type &p,
                       handle->GetCompletedParts()) {
          partIds.insert(p.second->GetPartId());
        }

        vector<int> completedPartIds(partIds.begin(), partIds.end());
        ClientError<QSError::Value> err = client->CompleteMultipartUpload(
            handle->GetObjectKey(), handle->GetMultiPartId(), completedPartIds);

        if (IsGoodQSError(err)) {
          handle->UpdateStatus(TransferStatus::Completed);
        } else {
          handle->UpdateStatus(TransferStatus::Failed);
          handle->SetError(err);
          Error(GetMessageForQSError(err));
        }
      } else {
        handle->UpdateStatus(TransferStatus::Failed);
      }
    }
  }
};

// --------------------------------------------------------------------------
shared_ptr<TransferHandle> QSTransferManager::DownloadFile(
    const string &filePath, off_t offset, uint64_t size,
    shared_ptr<iostream> bufStream, bool async) {
  // Drive::ReadFile has checked the object existence, so no check here.
  // Drive::ReadFile has already ajust the download size, so no ajust here.
  if (!bufStream) {
    DebugError("Null buffer stream parameter");
    return shared_ptr<TransferHandle>();
  }

  string bucket = ClientConfiguration::Instance().GetBucket();
  shared_ptr<TransferHandle> handle = make_shared<TransferHandle>(
      bucket, filePath, offset, size, TransferDirection::Download);
  handle->SetDownloadStream(bufStream);

  DoDownload(handle, async);
  return handle;
}

// --------------------------------------------------------------------------
shared_ptr<TransferHandle> QSTransferManager::RetryDownload(
    const shared_ptr<TransferHandle> &handle, shared_ptr<iostream> bufStream,
    bool async) {
  if (handle->GetStatus() == TransferStatus::InProgress ||
      handle->GetStatus() == TransferStatus::Completed ||
      handle->GetStatus() == TransferStatus::NotStarted) {
    DebugWarning("Input handle is not avaialbe to retry");
    return handle;
  }

  if (handle->GetStatus() == TransferStatus::Aborted) {
    return DownloadFile(handle->GetObjectKey(), handle->GetContentRangeBegin(),
                        handle->GetBytesTotalSize(), bufStream, async);
  } else {
    handle->UpdateStatus(TransferStatus::NotStarted);
    handle->Restart();
    DoDownload(handle, async);
    return handle;
  }
}

// --------------------------------------------------------------------------
shared_ptr<TransferHandle> QSTransferManager::UploadFile(
    const string &filePath, uint64_t fileSize, time_t fileMtimeSince,
    const shared_ptr<Cache> &cache, bool async) {
  string bucket = ClientConfiguration::Instance().GetBucket();
  shared_ptr<TransferHandle> handle = make_shared<TransferHandle>(
      bucket, filePath, 0, fileSize, TransferDirection::Upload);
  if (!cache) {
    DebugError("Null Cache input");
    return handle;
  }
  DoUpload(handle, cache, fileMtimeSince, async);

  return handle;
}

// --------------------------------------------------------------------------
shared_ptr<TransferHandle> QSTransferManager::RetryUpload(
    const shared_ptr<TransferHandle> &handle, time_t fileMtimeSince,
    const shared_ptr<Cache> &cache, bool async) {
  if (handle->GetStatus() == TransferStatus::InProgress ||
      handle->GetStatus() == TransferStatus::Completed ||
      handle->GetStatus() == TransferStatus::NotStarted) {
    DebugWarning("Input handle is not avaialbe to retry");
    return handle;
  }

  if (!cache) {
    DebugError("Null Cache input");
    return handle;
  }

  if (handle->GetStatus() == TransferStatus::Aborted) {
    return UploadFile(handle->GetObjectKey(), handle->GetBytesTotalSize(),
                      fileMtimeSince, cache, async);
  } else {
    handle->UpdateStatus(TransferStatus::NotStarted);
    handle->Restart();
    DoUpload(handle, cache, fileMtimeSince, async);
    return handle;
  }
}

// --------------------------------------------------------------------------
void QSTransferManager::AbortMultipartUpload(
    const shared_ptr<TransferHandle> &handle) {
  if (!handle->IsMultipart()) {
    DebugWarning("Unable to abort a non multipart upload");
    return;
  }

  handle->Cancle();
  handle->WaitUntilFinished();
  if (handle->GetStatus() == TransferStatus::Cancelled) {
    ClientError<QSError::Value> err = GetClient()->AbortMultipartUpload(
        handle->GetObjectKey(), handle->GetMultiPartId());
    if (IsGoodQSError(err)) {
      handle->UpdateStatus(TransferStatus::Aborted);
    } else {
      handle->SetError(err);
      Error(GetMessageForQSError(err));
    }
  }
}

// --------------------------------------------------------------------------
bool QSTransferManager::PrepareDownload(
    const shared_ptr<TransferHandle> &handle) {
  uint64_t bufferSize = GetBufferSize();
  assert(bufferSize > 0);
  if (!(bufferSize > 0)) {
    DebugError("Buffer size is less than 0");
    return false;
  }

  bool isRetry = handle->HasParts();
  if (isRetry) {
    BOOST_FOREACH(const PartIdToPartMapIterator::value_type &p,
                   handle->GetFailedParts()) {
      handle->AddQueuePart(p.second);
    }
  } else {
    // prepare part and add it into queue
    uint64_t totalTransferSize = handle->GetBytesTotalSize();
    size_t partCount = static_cast<size_t>(
        std::ceil(static_cast<long double>(totalTransferSize) /
                  static_cast<long double>(bufferSize)));
    handle->SetIsMultiPart(partCount > 1);
    for (size_t i = 1; i < partCount; ++i) {
      // part id, best progress in bytes, part size, range begin
      handle->AddQueuePart(make_shared<Part>(
          i, 0, bufferSize,
          handle->GetContentRangeBegin() + (i - 1) * bufferSize));
    }
    size_t sz = totalTransferSize - (partCount - 1) * bufferSize;
    handle->AddQueuePart(make_shared<Part>(
        partCount, 0, std::min(sz, static_cast<size_t>(bufferSize)),
        handle->GetContentRangeBegin() + (partCount - 1) * bufferSize));
  }
  return true;
}

// --------------------------------------------------------------------------
void QSTransferManager::DoSinglePartDownload(
    const shared_ptr<TransferHandle> &handle, bool async) {
  PartIdToPartMap queuedParts = handle->GetQueuedParts();
  assert(queuedParts.size() == 1);

  const shared_ptr<Part> &part = queuedParts.begin()->second;
  handle->AddPendingPart(part);
  ReceivedHandlerSingleDownload receivedHandler(handle, part);

  if (async) {
    GetExecutor()->SubmitAsyncPrioritized(
        bind(boost::type<void>(), receivedHandler, _1),
        bind(boost::type<pair<ClientError<QSError::Value>, string> >(),
             &QSTransferManager::SingleDownloadWrapper, this, _1, _2),
        handle, part);
  } else {
    receivedHandler(SingleDownloadWrapper(handle, part));
  }
}

// --------------------------------------------------------------------------
void QSTransferManager::DoMultiPartDownload(
    const shared_ptr<TransferHandle> &handle, bool async) {
  PartIdToPartMap queuedParts = handle->GetQueuedParts();
  PartIdToPartMapIterator ipart = queuedParts.begin();

  for (; ipart != queuedParts.end() && handle->ShouldContinue(); ++ipart) {
    const shared_ptr<Part> &part = ipart->second;
    Buffer buffer = GetBufferManager()->Acquire();
    if (!buffer) {
      DebugWarning("Unable to acquire resource, stop download");
      handle->ChangePartToFailed(part);
      handle->UpdateStatus(TransferStatus::Failed);
      handle->SetError(ClientError<QSError::Value>(
          QSError::NO_SUCH_MULTIPART_DOWNLOAD, "DoMultiPartDownload",
          QSErrorToString(QSError::NO_SUCH_MULTIPART_DOWNLOAD), false));
      break;
    }
    if (handle->ShouldContinue()) {
      part->SetDownloadPartStream(
          make_shared<IOStream>(buffer, part->GetSize()));
      handle->AddPendingPart(part);
      ReceivedHandlerMultipleDownload receivedHandler(handle, part,
                                                      GetBufferManager());

      if (async) {
        GetExecutor()->SubmitAsync(
            bind(boost::type<void>(), receivedHandler, _1),
            bind(boost::type<pair<ClientError<QSError::Value>, string> >(),
                 &QSTransferManager::MultipleDownloadWrapper, this, _1, _2),
            handle, part);
      } else {
        receivedHandler(MultipleDownloadWrapper(handle, part));
      }
    } else {
      GetBufferManager()->Release(buffer);
      break;
    }
  }

  for (; ipart != queuedParts.end(); ++ipart) {
    handle->ChangePartToFailed(ipart->second);
  }
}

// --------------------------------------------------------------------------
void QSTransferManager::DoDownload(const shared_ptr<TransferHandle> &handle,
                                   bool async) {
  handle->UpdateStatus(TransferStatus::InProgress);
  if (!PrepareDownload(handle)) {
    return;
  }
  if (handle->IsMultipart()) {
    DoMultiPartDownload(handle, async);
  } else {
    DoSinglePartDownload(handle, async);
  }
}

// --------------------------------------------------------------------------
bool QSTransferManager::PrepareUpload(
    const shared_ptr<TransferHandle> &handle) {
  uint64_t bufferSize = GetBufferSize();
  assert(bufferSize > 0);
  if (!(bufferSize > 0)) {
    DebugError("Buffer size is less than 0");
    return false;
  }

  bool isRetry = handle->HasParts();
  if (isRetry) {
    BOOST_FOREACH(const PartIdToPartMapIterator::value_type &p,
                   handle->GetFailedParts()) {
      handle->AddQueuePart(p.second);
    }
  } else {
    uint64_t totalTransferSize = handle->GetBytesTotalSize();
    if (totalTransferSize >=
        GetUploadMultipartThresholdSize()) {  // multiple upload
      handle->SetIsMultiPart(true);

      bool initSuccess = false;
      string uploadId;
      ClientError<QSError::Value> err = GetClient()->InitiateMultipartUpload(
          handle->GetObjectKey(), &uploadId);
      if (IsGoodQSError(err)) {
        handle->SetMultipartId(uploadId);
        initSuccess = true;
      } else {
        handle->SetError(err);
        handle->UpdateStatus(TransferStatus::Failed);
        Error(GetMessageForQSError(err));
        initSuccess = false;
      }
      if (!initSuccess) {
        return false;
      }

      size_t partCount = static_cast<size_t>(
          std::ceil(static_cast<long double>(totalTransferSize) /
                    static_cast<long double>(bufferSize)));
      size_t lastCuttingSize = totalTransferSize - (partCount - 1) * bufferSize;
      bool needAverageLastTwoPart =
          lastCuttingSize < GetUploadMultipartMinPartSize();

      size_t count = needAverageLastTwoPart ? partCount - 1 : partCount;
      for (size_t i = 1; i < count; ++i) {
        // part id, best progress in bytes, part size, range begin
        handle->AddQueuePart(make_shared<Part>(
            i, 0, bufferSize,
            handle->GetContentRangeBegin() + (i - 1) * bufferSize));
      }

      size_t sz = needAverageLastTwoPart ? (lastCuttingSize + bufferSize) / 2
                                         : lastCuttingSize;
      for (size_t i = count; i <= partCount; ++i) {
        handle->AddQueuePart(make_shared<Part>(i, 0, sz,
                                               handle->GetContentRangeBegin() +
                                                   (count - 1) * bufferSize +
                                                   (i - count) * sz));
      }
    } else {  // single upload
      handle->SetIsMultiPart(false);
      handle->AddQueuePart(make_shared<Part>(1, 0, totalTransferSize,
                                             handle->GetContentRangeBegin()));
    }
  }
  return true;
}

// --------------------------------------------------------------------------
void QSTransferManager::DoSinglePartUpload(
    const shared_ptr<TransferHandle> &handle, const shared_ptr<Cache> &cache,
    time_t mtimeSince, bool async) {
  PartIdToPartMap queuedParts = handle->GetQueuedParts();
  assert(queuedParts.size() == 1);

  const shared_ptr<Part> &part = queuedParts.begin()->second;
  uint64_t fileSize = handle->GetBytesTotalSize();
  Buffer buf = Buffer(new vector<char>(fileSize));
  string objKey = handle->GetObjectKey();
  pair<size_t, ContentRangeDeque> res =
      cache->Read(objKey, 0, fileSize, &(*buf)[0], mtimeSince);
  size_t readSize = res.first;
  if (readSize != fileSize) {
    DebugError("Fail to read cache [file:offset:len:readsize=" + objKey +
               ":0:" + to_string(fileSize) + ":" + to_string(readSize) +
               "], stop upload");
    handle->ChangePartToFailed(part);
    handle->UpdateStatus(TransferStatus::Failed);
    handle->SetError(ClientError<QSError::Value>(
        QSError::NO_SUCH_UPLOAD, "DoSinglePartUpload",
        QSErrorToString(QSError::NO_SUCH_UPLOAD), false));

    return;
  }

  shared_ptr<IOStream> stream =
      shared_ptr<IOStream>(new IOStream(buf, fileSize));
  handle->AddPendingPart(part);
  ReceivedHandlerSingleUpload receivedHandler(handle, part, stream);

  if (async) {
    GetExecutor()->SubmitAsyncPrioritized(
        bind(boost::type<void>(), receivedHandler, _1),
        bind(boost::type<ClientError<QSError::Value> >(),
             &QSTransferManager::SingleUploadWrapper, this, _1, _2),
        handle, stream);
  } else {
    receivedHandler(SingleUploadWrapper(handle, stream));
  }
}

// --------------------------------------------------------------------------
void QSTransferManager::DoMultiPartUpload(
    const shared_ptr<TransferHandle> &handle, const shared_ptr<Cache> &cache,
    time_t mtimeSince, bool async) {
  PartIdToPartMap queuedParts = handle->GetQueuedParts();
  string objKey = handle->GetObjectKey();
  PartIdToPartMapIterator ipart = queuedParts.begin();
  for (; ipart != queuedParts.end() && handle->ShouldContinue(); ++ipart) {
    const shared_ptr<Part> &part = ipart->second;
    Buffer buffer = GetBufferManager()->Acquire();
    if (!buffer) {
      DebugWarning("Unable to acquire resource, stop upload");
      handle->ChangePartToFailed(part);
      handle->UpdateStatus(TransferStatus::Failed);
      handle->SetError(ClientError<QSError::Value>(
          QSError::NO_SUCH_MULTIPART_UPLOAD, "DoMultiPartUpload",
          QSErrorToString(QSError::NO_SUCH_MULTIPART_UPLOAD), false));
      break;
    }

    pair<size_t, ContentRangeDeque> res =
        cache->Read(objKey, part->GetRangeBegin(), part->GetSize(),
                    &(*buffer)[0], mtimeSince);
    size_t readSize = res.first;
    if (readSize != part->GetSize()) {
      DebugError("Fail to read cache [file:offset:len:readsize=" + objKey +
                 ":" + to_string(part->GetRangeBegin()) + ":" +
                 to_string(part->GetSize()) + ":" + to_string(readSize) +
                 "], stop upload");

      handle->ChangePartToFailed(part);
      handle->UpdateStatus(TransferStatus::Failed);
      handle->SetError(ClientError<QSError::Value>(
          QSError::NO_SUCH_MULTIPART_UPLOAD, "DoMultiPartUpload",
          QSErrorToString(QSError::NO_SUCH_MULTIPART_UPLOAD), false));
      GetBufferManager()->Release(buffer);
      break;
    }

    if (handle->ShouldContinue()) {
      shared_ptr<IOStream> stream =
          make_shared<IOStream>(buffer, part->GetSize());
      handle->AddPendingPart(part);
      ReceivedHandlerMultipleUpload receivedHandler(
          handle, part, stream, GetBufferManager(), GetClient());

      if (async) {
        GetExecutor()->SubmitAsync(
            bind(boost::type<void>(), receivedHandler, _1),
            bind(boost::type<ClientError<QSError::Value> >(),
                 &QSTransferManager::MultipleUploadWrapper, this, _1, _2, _3),
            handle, part, stream);
      } else {
        receivedHandler(MultipleUploadWrapper(handle, part, stream));
      }

    } else {
      GetBufferManager()->Release(buffer);
      break;
    }
  }

  for (; ipart != queuedParts.end(); ++ipart) {
    handle->ChangePartToFailed(ipart->second);
  }
}

// --------------------------------------------------------------------------
void QSTransferManager::DoUpload(const shared_ptr<TransferHandle> &handle,
                                 const shared_ptr<Cache> &cache,
                                 time_t mtimeSince, bool async) {
  handle->UpdateStatus(TransferStatus::InProgress);
  if (!PrepareUpload(handle)) {
    return;
  }
  if (handle->IsMultipart()) {
    DoMultiPartUpload(handle, cache, mtimeSince, async);
  } else {
    DoSinglePartUpload(handle, cache, mtimeSince, async);
  }
}

// --------------------------------------------------------------------------
pair<ClientError<QSError::Value>, string>
QSTransferManager::SingleDownloadWrapper(
    const shared_ptr<TransferHandle> &handle, const shared_ptr<Part> &part) {
  string eTag;
  ClientError<QSError::Value> err = GetClient()->DownloadFile(
      handle->GetObjectKey(), handle->GetDownloadStream(),
      BuildRequestRange(part->GetRangeBegin(), part->GetSize()), &eTag);
  return make_pair(err, eTag);
}

// --------------------------------------------------------------------------
pair<ClientError<QSError::Value>, string>
QSTransferManager::MultipleDownloadWrapper(
    const shared_ptr<TransferHandle> &handle, const shared_ptr<Part> &part) {
  string eTag;
  ClientError<QSError::Value> err = GetClient()->DownloadFile(
      handle->GetObjectKey(), part->GetDownloadPartStream(),
      BuildRequestRange(part->GetRangeBegin(), part->GetSize()), &eTag);
  return make_pair(err, eTag);
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSTransferManager::SingleUploadWrapper(
    const shared_ptr<TransferHandle> &handle,
    const shared_ptr<IOStream> &stream) {
  return GetClient()->UploadFile(handle->GetObjectKey(),
                                 handle->GetBytesTotalSize(), stream);
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> QSTransferManager::MultipleUploadWrapper(
    const shared_ptr<TransferHandle> &handle, const shared_ptr<Part> &part,
    const shared_ptr<IOStream> &stream) {
  return GetClient()->UploadMultipart(
      handle->GetObjectKey(), handle->GetMultiPartId(), part->GetPartId(),
      part->GetSize(), stream);
}

}  // namespace Client
}  // namespace QS
