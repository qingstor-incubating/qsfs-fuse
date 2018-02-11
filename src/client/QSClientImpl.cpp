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

#include "client/QSClientImpl.h"

#include <assert.h>

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "qingstor/Bucket.h"
#include "qingstor/HttpCommon.h"
#include "qingstor/QingStor.h"
#include "qingstor/QsConfig.h"
#include "qingstor/QsErrors.h"  // for sdk QsError
#include "qingstor/Types.h"     // for sdk QsOutput

#include "base/LogMacros.h"
#include "client/ClientConfiguration.h"
#include "client/QSClient.h"
#include "client/QSError.h"
#include "client/Utils.h"

namespace QS {

namespace Client {

using boost::shared_ptr;
using QingStor::AbortMultipartUploadInput;
using QingStor::AbortMultipartUploadOutput;
using QingStor::Bucket;
using QingStor::CompleteMultipartUploadInput;
using QingStor::CompleteMultipartUploadOutput;
using QingStor::DeleteObjectInput;
using QingStor::DeleteObjectOutput;
using QingStor::GetBucketStatisticsInput;
using QingStor::GetBucketStatisticsOutput;
using QingStor::GetObjectInput;
using QingStor::GetObjectOutput;
using QingStor::HeadBucketInput;
using QingStor::HeadBucketOutput;
using QingStor::HeadObjectInput;
using QingStor::HeadObjectOutput;
using QingStor::Http::HttpResponseCode;
using QingStor::InitiateMultipartUploadInput;
using QingStor::InitiateMultipartUploadOutput;
using QingStor::ListObjectsInput;
using QingStor::ListObjectsOutput;
using QingStor::PutObjectInput;
using QingStor::PutObjectOutput;
using QingStor::QsOutput;
using QingStor::UploadMultipartInput;
using QingStor::UploadMultipartOutput;
using QS::Client::Utils::ParseRequestContentRange;
using std::string;
using std::vector;

namespace {

// --------------------------------------------------------------------------
ClientError<QSError::Value> BuildQSError(QsError sdkErr,
                                         const string &exceptionName,
                                         const QsOutput &output,
                                         bool retriable) {
  HttpResponseCode rspCode = const_cast<QsOutput &>(output).GetResponseCode();
  QSError::Value err = SDKResponseToQSError(sdkErr, rspCode);

  if (sdkErr == QS_ERR_UNEXCEPTED_RESPONSE) {
    // it seems sdk response error info contain empty content,
    // so we print repsonde code at beginning
    string errMsg = SDKResponseCodeToString(rspCode);
    QingStor::ResponseErrorInfo errInfo = output.GetResponseErrInfo();
    errMsg += "[code:" + errInfo.code;
    errMsg += "; message:" + errInfo.message;
    errMsg += "; request:" + errInfo.requestID;
    errMsg += "; url:" + errInfo.url;
    errMsg += "]";
    return ClientError<QSError::Value>(err, exceptionName, errMsg, retriable);
  } else {
    return ClientError<QSError::Value>(
        err, exceptionName, SDKResponseCodeToString(rspCode), retriable);
  }
}

}  // namespace

// --------------------------------------------------------------------------
QSClientImpl::QSClientImpl() : ClientImpl() {
  if (!m_bucket) {
    const ClientConfiguration &clientConfig = ClientConfiguration::Instance();
    const shared_ptr<QingStor::QsConfig> &qsConfig =
        QSClient::GetQingStorConfig();
    m_bucket = shared_ptr<Bucket>(new Bucket(
        *qsConfig, clientConfig.GetBucket(), clientConfig.GetZone()));
  }
}

// --------------------------------------------------------------------------
GetBucketStatisticsOutcome QSClientImpl::GetBucketStatistics() const {
  GetBucketStatisticsInput input;  // dummy input
  GetBucketStatisticsOutput output;
  QsError sdkErr = m_bucket->GetBucketStatistics(input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return GetBucketStatisticsOutcome(output);
  } else {
    string exceptionName = "QingStorGetBucketStatistics";
    return GetBucketStatisticsOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
HeadBucketOutcome QSClientImpl::HeadBucket() const {
  HeadBucketInput input;  // dummy input
  HeadBucketOutput output;
  QsError sdkErr = m_bucket->HeadBucket(input, output);

  string exceptionName = "QingStorHeadBucket";
  HttpResponseCode responseCode = output.GetResponseCode();
  if (responseCode == QingStor::Http::NOT_FOUND) {
    return HeadBucketOutcome(ClientError<QSError::Value>(
        QSError::NOT_FOUND, exceptionName,
        SDKResponseCodeToString(responseCode), false));
  }

  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return HeadBucketOutcome(output);
  } else {
    return HeadBucketOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
ListObjectsOutcome QSClientImpl::ListObjects(ListObjectsInput *input,
                                             bool *resultTruncated,
                                             uint64_t *resCount,
                                             uint64_t maxCount) const {
  string exceptionName = "QingStorListObjects";
  if (input == NULL) {
    return ListObjectsOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null ListObjectsInput", false));
  }
  exceptionName.append(" prefix=");
  exceptionName.append(input->GetPrefix());

  if (input->GetLimit() <= 0) {
    return ListObjectsOutcome(ClientError<QSError::Value>(
        QSError::NO_SUCH_LIST_OBJECTS, exceptionName,
        "ListObjectsInput with negative or zero count limit", false));
  }

  if (resultTruncated != NULL) {
    *resultTruncated = false;
  }
  if (resCount != NULL) {
    *resCount = 0;
  }

  bool listAllObjects = maxCount == 0;
  uint64_t count = 0;
  bool responseTruncated = true;
  vector<ListObjectsOutput> result;
  do {
    if (!listAllObjects) {
      int remainingCount = static_cast<int>(maxCount - count);
      if (remainingCount < input->GetLimit()) {
        input->SetLimit(remainingCount);
      }
    }

    ListObjectsOutput output;
    QsError sdkErr = m_bucket->ListObjects(*input, output);

    HttpResponseCode responseCode = output.GetResponseCode();
    if (SDKResponseSuccess(sdkErr, responseCode)) {
      count += output.GetKeys().size();
      count += output.GetCommonPrefixes().size();
      responseTruncated = !output.GetNextMarker().empty();
      if (responseTruncated) {
        input->SetMarker(output.GetNextMarker());
      }
      result.push_back(output);
    } else {
      return ListObjectsOutcome(BuildQSError(
          sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
    }
  } while (responseTruncated && (listAllObjects || count < maxCount));
  if (resultTruncated != NULL) {
    *resultTruncated = responseTruncated;
  }
  if (resCount != NULL) {
    *resCount = count;
  }
  return ListObjectsOutcome(result);
}

// --------------------------------------------------------------------------
DeleteObjectOutcome QSClientImpl::DeleteObject(const string &objKey) const {
  string exceptionName = "QingStorDeleteObject";
  if (objKey.empty()) {
    return DeleteObjectOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }

  DeleteObjectInput input;  // dummy input
  DeleteObjectOutput output;
  QsError sdkErr = m_bucket->DeleteObject(objKey, input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return DeleteObjectOutcome(output);
  } else {
    exceptionName.append(" object=");
    exceptionName.append(objKey);
    return DeleteObjectOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
GetObjectOutcome QSClientImpl::GetObject(const string &objKey,
                                         GetObjectInput *input) const {
  string exceptionName = "QingStorGetObject";
  if (objKey.empty()) {
    return GetObjectOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return GetObjectOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null GetObjectInput", false));
  }

  GetObjectOutput output;
  QsError sdkErr = m_bucket->GetObject(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    bool askPartialContent = !input->GetRange().empty();
    if (askPartialContent) {
      // qs sdk specification: if request set with range parameter, then
      // response successful with code 206 (Partial Content)
      if (output.GetResponseCode() != QingStor::Http::PARTIAL_CONTENT) {
        Warning("Request for " + input->GetRange() +
                ", but response is not 206 (Partial Content)");
        return GetObjectOutcome(
            BuildQSError(sdkErr, exceptionName, output, true));
      } else {
        size_t reqLen = ParseRequestContentRange(input->GetRange()).second;
        size_t rspLen = output.GetContentLength();
        DebugWarningIf(rspLen < reqLen,
                       "[content range request:response=" + input->GetRange() +
                           ":" + output.GetContentRange() + "]");
      }
    }
    return GetObjectOutcome(output);
  } else {
    return GetObjectOutcome(BuildQSError(sdkErr, exceptionName, output,
                                         SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
HeadObjectOutcome QSClientImpl::HeadObject(const string &objKey,
                                           HeadObjectInput *input) const {
  string exceptionName = "QingStorHeadObject";
  if (objKey.empty()) {
    return HeadObjectOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return HeadObjectOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null HeadObjectInput", false));
  }

  HeadObjectOutput output;
  QsError sdkErr = m_bucket->HeadObject(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return HeadObjectOutcome(output);
  } else {
    return HeadObjectOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
PutObjectOutcome QSClientImpl::PutObject(const string &objKey,
                                         PutObjectInput *input) const {
  string exceptionName = "QingStorPutObject";
  if (objKey.empty()) {
    return PutObjectOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return PutObjectOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null PutObjectInput", false));
  }

  PutObjectOutput output;
  QsError sdkErr = m_bucket->PutObject(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return PutObjectOutcome(output);
  } else {
    return PutObjectOutcome(BuildQSError(sdkErr, exceptionName, output,
                                         SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
InitiateMultipartUploadOutcome QSClientImpl::InitiateMultipartUpload(
    const string &objKey, InitiateMultipartUploadInput *input) const {
  string exceptionName = "QingStorInitiateMultipartUpload";
  if (objKey.empty()) {
    return InitiateMultipartUploadOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return InitiateMultipartUploadOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName,
        "Null InitiateMultipartUploadInput", false));
  }

  InitiateMultipartUploadOutput output;
  QsError sdkErr = m_bucket->InitiateMultipartUpload(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return InitiateMultipartUploadOutcome(output);
  } else {
    return InitiateMultipartUploadOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
UploadMultipartOutcome QSClientImpl::UploadMultipart(
    const string &objKey, UploadMultipartInput *input) const {
  string exceptionName = "QingStorUploadMultipart";
  if (objKey.empty()) {
    return UploadMultipartOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return UploadMultipartOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null UploadMultipartInput", false));
  }

  UploadMultipartOutput output;
  QsError sdkErr = m_bucket->UploadMultipart(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return UploadMultipartOutcome(output);
  } else {
    return UploadMultipartOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
CompleteMultipartUploadOutcome QSClientImpl::CompleteMultipartUpload(
    const string &objKey, CompleteMultipartUploadInput *input) const {
  string exceptionName = "QingStorCompleteMultipartUpload";
  if (objKey.empty()) {
    return CompleteMultipartUploadOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return CompleteMultipartUploadOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName,
        "Null CompleteMutlipartUploadInput", false));
  }

  CompleteMultipartUploadOutput output;
  QsError sdkErr = m_bucket->CompleteMultipartUpload(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return CompleteMultipartUploadOutcome(output);
  } else {
    return CompleteMultipartUploadOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
AbortMultipartUploadOutcome QSClientImpl::AbortMultipartUpload(
    const string &objKey, AbortMultipartUploadInput *input) const {
  string exceptionName = "QingStorAbortMultipartUpload";
  if (objKey.empty()) {
    return AbortMultipartUploadOutcome(ClientError<QSError::Value>(
        QSError::PARAMETER_MISSING, exceptionName, "Empty ObjectKey", false));
  }
  exceptionName.append(" object=");
  exceptionName.append(objKey);
  if (input == NULL) {
    return AbortMultipartUploadOutcome(
        ClientError<QSError::Value>(QSError::PARAMETER_MISSING, exceptionName,
                                    "Null AbortMultipartUploadInput", false));
  }

  AbortMultipartUploadOutput output;
  QsError sdkErr = m_bucket->AbortMultipartUpload(objKey, *input, output);

  HttpResponseCode responseCode = output.GetResponseCode();
  if (SDKResponseSuccess(sdkErr, responseCode)) {
    return AbortMultipartUploadOutcome(output);
  } else {
    return AbortMultipartUploadOutcome(BuildQSError(
        sdkErr, exceptionName, output, SDKShouldRetry(sdkErr, responseCode)));
  }
}

// --------------------------------------------------------------------------
void QSClientImpl::SetBucket(const shared_ptr<Bucket> &bucket) {
  assert(bucket);
  FatalIf(!bucket, "Setting a NULL bucket!");
  m_bucket = bucket;
}

}  // namespace Client
}  // namespace QS
