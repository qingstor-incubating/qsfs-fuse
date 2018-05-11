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

#include "client/QSError.h"

#include <utility>

#include "boost/exception/to_string.hpp"

#include "qingstor/HttpCommon.h"
#include "qingstor/QsErrors.h"

#include "base/HashUtils.h"

namespace QS {

namespace Client {

using boost::to_string;
using QingStor::Http::HttpResponseCode;
using std::make_pair;
using std::pair;
using std::string;

namespace {

bool SDKResponseCodeSuccess(HttpResponseCode code) {
  using namespace QingStor::Http;  // NOLINT
  HttpResponseCode codes[] = {
      // keep in sorted order
      CONTINUE,         //"Continue"                    100
      PROCESSING,       //"Processing"                  102
      OK,               //"Ok"                          200
      CREATED,          //"Created"                     201
      ACCEPTED,         //"Accepted"                    202
      NO_CONTENT,       //"NoContent"                   204
      PARTIAL_CONTENT,  //"PartialContent"              206
      FOUND,            //"Found"                       302
      NOT_MODIFIED,      //"NotModified"                 304
  };

  int n = sizeof(codes) / sizeof(codes[0]);
  // binary search
  int low = 0;
  int high = n - 1;
  bool found = false;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (code == codes[mid]) {
      found = true;
      break;
    }
    if (static_cast<int>(code) < static_cast<int>(codes[mid])) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return found;
}

}  // namespace

// --------------------------------------------------------------------------
QSError::Value StringToQSError(const char *err) {
  if (strcmp(err, "Unknow") == 0) {
    return QSError::UNKNOWN;
  }
  if (strcmp(err, "Good") == 0) {
    return QSError::GOOD;
  }
  if (strcmp(err, "NoSuchListObjects") == 0) {
    return QSError::NO_SUCH_LIST_OBJECTS;
  }
  if (strcmp(err, "NoSuchMultipartDownload") == 0) {
    return QSError::NO_SUCH_MULTIPART_DOWNLOAD;
  }
  if (strcmp(err, "NoSuchMultipartUpload") == 0) {
    return QSError::NO_SUCH_MULTIPART_UPLOAD;
  }
  if (strcmp(err, "NoSuchUpload") == 0) {
    return QSError::NO_SUCH_UPLOAD;
  }
  if (strcmp(err, "ParameterMissing") == 0) {
    return QSError::PARAMETER_MISSING;
  }
  if (strcmp(err, "SDKConfigureFileInvalid") == 0) {
    return QSError::SDK_CONFIGURE_FILE_INAVLID;
  }
  if (strcmp(err, "SDKNoRequiredParameter") == 0) {
    return QSError::SDK_NO_REQUIRED_PARAMETER;
  }
  if (strcmp(err, "SDKRequestSendError") == 0) {
    return QSError::SDK_REQUEST_SEND_ERROR;
  }
  if (strcmp(err, "SDKUnexpectedResponse") == 0) {
    return QSError::SDK_UNEXPECTED_RESPONSE;
  }
  if (strcmp(err, "SDKSignWithInvalidKey") == 0) {
    return QSError::SDK_SIGN_WITH_INVAILD_KEY;
  }
  if (strcmp(err, "NotFound") == 0) {
    return QSError::NOT_FOUND;
  }

  return QSError::UNKNOWN;
}

// --------------------------------------------------------------------------
std::string QSErrorToString(QSError::Value err) {
  pair<QSError::Value, const char *> errToNames[] = {
      // keep in sorted oreder
      make_pair(QSError::UNKNOWN, "Unknow"),
      make_pair(QSError::GOOD, "Good"),
      make_pair(QSError::NO_SUCH_LIST_OBJECTS, "NoSuchListObjects"),
      make_pair(QSError::NO_SUCH_MULTIPART_DOWNLOAD, "NoSuchMultipartDownload"),
      make_pair(QSError::NO_SUCH_MULTIPART_UPLOAD, "NoSuchMultipartUpload"),
      make_pair(QSError::NO_SUCH_UPLOAD, "NoSuchUpload"),
      make_pair(QSError::PARAMETER_MISSING, "ParameterMissing"),
      make_pair(QSError::SDK_CONFIGURE_FILE_INAVLID, "SDKConfigureFileInvalid"),
      make_pair(QSError::SDK_NO_REQUIRED_PARAMETER, "SDKNoRequiredParameter"),
      make_pair(QSError::SDK_REQUEST_SEND_ERROR, "SDKRequestSendError"),
      make_pair(QSError::SDK_UNEXPECTED_RESPONSE, "SDKUnexpectedResponse"),
      make_pair(QSError::SDK_SIGN_WITH_INVAILD_KEY, "SDKSignWithInvalidKey"),
      make_pair(QSError::NOT_FOUND, "NotFound"),
  };

  int n = sizeof(errToNames) / sizeof(errToNames[0]);
  // binary search
  int low = 0;
  int high = n - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (err == errToNames[mid].first) {
      return errToNames[mid].second;
    }
    if (static_cast<int>(err) < static_cast<int>(errToNames[mid].first)) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return "Unknow";
}

// --------------------------------------------------------------------------
ClientError<QSError::Value> GetQSErrorForCode(const string &errorCode) {
  return ClientError<QSError::Value>(StringToQSError(errorCode.c_str()),false);
}

// --------------------------------------------------------------------------
std::string GetMessageForQSError(const ClientError<QSError::Value> &error) {
  return QSErrorToString(error.GetError()) + ", " + error.GetExceptionName() +
         ":" + error.GetMessage();
}

// --------------------------------------------------------------------------
bool IsGoodQSError(const ClientError<QSError::Value> &error){
  return error.GetError() == QSError::GOOD;
}

// --------------------------------------------------------------------------
QSError::Value SDKErrorToQSError(QsError sdkErr) {
  pair<QsError, QSError::Value> pairs[] = {
      // keep in sorted order
      make_pair(QS_ERR_NO_ERROR, QSError::GOOD),
      make_pair(QS_ERR_INVAILD_CONFIG_FILE, QSError::SDK_CONFIGURE_FILE_INAVLID),
      make_pair(QS_ERR_NO_REQUIRED_PARAMETER, QSError::SDK_NO_REQUIRED_PARAMETER),
      make_pair(QS_ERR_SEND_REQUEST_ERROR, QSError::SDK_REQUEST_SEND_ERROR),
      make_pair(QS_ERR_UNEXCEPTED_RESPONSE, QSError::SDK_UNEXPECTED_RESPONSE),
      make_pair(QS_ERR_SIGN_WITH_INVAILD_KEY, QSError::SDK_SIGN_WITH_INVAILD_KEY),
  };

  int n = sizeof(pairs) / sizeof(pairs[0]);
  // binary search
  int low = 0;
  int high = n - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (sdkErr == pairs[mid].first) {
      return pairs[mid].second;
    }
    if (static_cast<int>(sdkErr) < static_cast<int>(pairs[mid].first)) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return QSError::UNKNOWN;
}

// --------------------------------------------------------------------------
QSError::Value SDKResponseToQSError(QsError sdkErr, HttpResponseCode code) {
  using namespace QingStor::Http;  // NOLINT
  QSError::Value err = SDKErrorToQSError(sdkErr);
  if (err != QSError::SDK_UNEXPECTED_RESPONSE) {
    return err;
  }

  if (code == NOT_FOUND ) {
    return QSError::NOT_FOUND;
  } else if (SDKResponseCodeSuccess(code)) {
    return QSError::GOOD;
  } else {
    return QSError::SDK_UNEXPECTED_RESPONSE;
  }
}

// --------------------------------------------------------------------------
bool SDKShouldRetry(QsError sdkErr, QingStor::Http::HttpResponseCode code){
  return false;  // as we not count on sdk retry
}

// --------------------------------------------------------------------------
bool SDKResponseSuccess(QsError sdkErr, HttpResponseCode code) {
  // sdk return NO_ERROR if response is expected as api specs
  // and return UNEXPECTED_RESPONSE if not expected as api specs;
  // but api specs is not completed, may requests only expect 200 response
  // so we take some addtional handles here
  return sdkErr == QS_ERR_NO_ERROR ||  // response is expected as api specs
         (sdkErr == QS_ERR_UNEXCEPTED_RESPONSE && SDKResponseCodeSuccess(code));
}

// --------------------------------------------------------------------------
string SDKResponseCodeToName(HttpResponseCode code) {
  using namespace QingStor::Http;  // NOLINT
  pair<HttpResponseCode, const char *> codeToNames[] = {
      // keep in sorted order
      make_pair(REQUEST_NOT_MADE, "RequestNotMade"),         // 0
      make_pair(CONTINUE, "Continue"),                       // 100
      make_pair(SWITCHING_PROTOCOLS, "SwitchingProtocols"),  // 101
      make_pair(PROCESSING, "Processing"),                   // 102
      make_pair(OK, "Ok"),                                   // 200
      make_pair(CREATED, "Created"),                         // 201
      make_pair(ACCEPTED, "Accepted"),                       // 202
      make_pair(NON_AUTHORITATIVE_INFORMATION,
                "NonAuthoritativeInformation"),                          // 203
      make_pair(NO_CONTENT, "NoContent"),                                // 204
      make_pair(RESET_CONTENT, "ResetContent"),                          // 205
      make_pair(PARTIAL_CONTENT, "PartialContent"),                      // 206
      make_pair(MULTI_STATUS, "MultiStatus"),                            // 207
      make_pair(ALREADY_REPORTED, "AlreadyReported"),                    // 208
      make_pair(IM_USED, "IMUsed"),                                      // 226
      make_pair(MULTIPLE_CHOICES, "MultipleChoices"),                    // 300
      make_pair(MOVED_PERMANENTLY, "MovedPermanently"),                  // 301
      make_pair(FOUND, "Found"),                                         // 302
      make_pair(SEE_OTHER, "SeeOther"),                                  // 303
      make_pair(NOT_MODIFIED, "NotModified"),                            // 304
      make_pair(USE_PROXY, "UseProxy"),                                  // 305
      make_pair(SWITCH_PROXY, "SwitchProxy"),                            // 306
      make_pair(TEMPORARY_REDIRECT, "TemporaryRedirect"),                // 307
      make_pair(PERMANENT_REDIRECT, "PermanentRedirect"),                // 308
      make_pair(BAD_REQUEST, "BadRequest"),                              // 400
      make_pair(UNAUTHORIZED_OR_EXPIRED, "UnauthorizedOrExpired"),       // 401
      make_pair(DELINQUENT_ACCOUNT, "DelinquentAccount"),                // 402
      make_pair(FORBIDDEN, "Forbidden"),                                 // 403
      make_pair(NOT_FOUND, "NotFound"),                                  // 404
      make_pair(METHOD_NOT_ALLOWED, "MethodNotAllowed"),                 // 405
      make_pair(CONFLICT, "Conflict"),                                   // 409
      make_pair(PRECONDITION_FAILED, "PerconditionFailed"),              // 412
      make_pair(INVALID_RANGE, "InvalidRange"),                          // 416
      make_pair(TOO_MANY_REQUESTS, "TooManyRequests"),                   // 429
      make_pair(INTERNAL_SERVER_ERROR, "InternalServerError"),           // 500
      make_pair(SERVICE_UNAVAILABLE, "ServiceUnavailable"),              // 503
      make_pair(GATEWAY_TIMEOUT, "GatewayTimeout"),                      // 504
      make_pair(HTTP_VERSION_NOT_SUPPORTED, "HttpVersionNotSupported"),  // 505
      make_pair(VARIANT_ALSO_NEGOTIATES, "VariantAlsoNegotiates"),       // 506
      make_pair(INSUFFICIENT_STORAGE, "InsufficientStorage"),            // 506
      make_pair(LOOP_DETECTED, "LoopDetected"),                          // 508
      make_pair(BANDWIDTH_LIMIT_EXCEEDED, "BandwithLimitExceeded"),      // 509
      make_pair(NOT_EXTENDED, "NotExtended"),                            // 510
      make_pair(NETWORK_AUTHENTICATION_REQUIRED,
                "NetworkAuthenticationRequired"),                   // 511
      make_pair(NETWORK_READ_TIMEOUT, "NetworkReadTimeout"),        // 598
      make_pair(NETWORK_CONNECT_TIMEOUT, "NetworkConnectTimeout"),  // 599
  };

  int n = sizeof(codeToNames) / sizeof(codeToNames[0]);
  // binary search
  int low = 0;
  int high = n - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (code == codeToNames[mid].first) {
      return codeToNames[mid].second;
    }
    if (static_cast<int>(code) < static_cast<int>(codeToNames[mid].first)) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return "UnknownQingStorResponseCode";
}

// --------------------------------------------------------------------------
int SDKResponseCodeToInt(HttpResponseCode code) {
  using namespace QingStor::Http;
  pair<HttpResponseCode, int> codeToNums[] = {
      // keep in sorted order
      make_pair(REQUEST_NOT_MADE, 0),
      make_pair(CONTINUE, 100),
      make_pair(SWITCHING_PROTOCOLS, 101),
      make_pair(PROCESSING, 102),
      make_pair(OK, 200),
      make_pair(CREATED, 201),
      make_pair(ACCEPTED, 202),
      make_pair(NON_AUTHORITATIVE_INFORMATION, 203),
      make_pair(NO_CONTENT, 204),
      make_pair(RESET_CONTENT, 205),
      make_pair(PARTIAL_CONTENT, 206),
      make_pair(MULTI_STATUS, 207),
      make_pair(ALREADY_REPORTED, 208),
      make_pair(IM_USED, 226),
      make_pair(MULTIPLE_CHOICES, 300),
      make_pair(MOVED_PERMANENTLY, 301),
      make_pair(FOUND, 302),
      make_pair(SEE_OTHER, 303),
      make_pair(NOT_MODIFIED, 304),
      make_pair(USE_PROXY, 305),
      make_pair(SWITCH_PROXY, 306),
      make_pair(TEMPORARY_REDIRECT, 307),
      make_pair(PERMANENT_REDIRECT, 308),
      make_pair(BAD_REQUEST, 400),
      make_pair(UNAUTHORIZED_OR_EXPIRED, 401),
      make_pair(DELINQUENT_ACCOUNT, 402),
      make_pair(FORBIDDEN, 403),
      make_pair(NOT_FOUND, 404),
      make_pair(METHOD_NOT_ALLOWED, 405),
      make_pair(CONFLICT, 409),
      make_pair(PRECONDITION_FAILED, 412),
      make_pair(INVALID_RANGE, 416),
      make_pair(TOO_MANY_REQUESTS, 429),
      make_pair(INTERNAL_SERVER_ERROR, 500),
      make_pair(SERVICE_UNAVAILABLE, 503),
      make_pair(GATEWAY_TIMEOUT, 504),
      make_pair(HTTP_VERSION_NOT_SUPPORTED, 505),
      make_pair(VARIANT_ALSO_NEGOTIATES, 506),
      make_pair(INSUFFICIENT_STORAGE, 506),
      make_pair(LOOP_DETECTED, 508),
      make_pair(BANDWIDTH_LIMIT_EXCEEDED, 509),
      make_pair(NOT_EXTENDED, 510),
      make_pair(NETWORK_AUTHENTICATION_REQUIRED, 511),
      make_pair(NETWORK_READ_TIMEOUT, 598),
      make_pair(NETWORK_CONNECT_TIMEOUT, 599),
  };

  int n = sizeof(codeToNums) / sizeof(codeToNums[0]);
  // binary search
  int low = 0;
  int high = n - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (code == codeToNums[mid].first) {
      return codeToNums[mid].second;
    }
    if (static_cast<int>(code) < static_cast<int>(codeToNums[mid].first)) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  return -1;
}

// --------------------------------------------------------------------------
string SDKResponseCodeToString(HttpResponseCode code) {
  return SDKResponseCodeToName(code) + "(" +
         to_string(SDKResponseCodeToInt(code)) + ")";
}

}  // namespace Client
}  // namespace QS

