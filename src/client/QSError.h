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

#ifndef QSFS_CLIENT_QSERROR_H_
#define QSFS_CLIENT_QSERROR_H_

#include <string>

#include "qingstor/HttpCommon.h"
#include "qingstor/QsErrors.h"

#include "client/ClientError.hpp"

namespace QS {

namespace Client {

struct QSError {
  enum Value {
    UNKNOWN,
    GOOD,

    // check sdk request
    NO_SUCH_LIST_OBJECTS,
    NO_SUCH_MULTIPART_DOWNLOAD,
    NO_SUCH_MULTIPART_UPLOAD,
    NO_SUCH_UPLOAD,
    PARAMETER_MISSING,

    // sdk error
    SDK_CONFIGURE_FILE_INAVLID,  // error when loading config file
    SDK_NO_REQUIRED_PARAMETER,   // request not send as missing required
                                 // parameters accoriding api specs
    SDK_REQUEST_SEND_ERROR,      // request send but get no response
    SDK_UNEXPECTED_RESPONSE,     // sdk get response but is not expected by api
                                 // specs
    // QS_ERR_NO_ERROR means response is expected by api specs

    // specifics for http response
    NOT_FOUND  // Not Found (404)
  };
};

QSError::Value StringToQSError(const std::string &errorCode);
std::string QSErrorToString(QSError::Value err);

ClientError<QSError::Value> GetQSErrorForCode(const std::string &errorCode);
std::string GetMessageForQSError(const ClientError<QSError::Value> &error);
bool IsGoodQSError(const ClientError<QSError::Value> &error);

QSError::Value SDKErrorToQSError(QsError sdkErr);
QSError::Value SDKResponseToQSError(QsError sdkErr,
                                    QingStor::Http::HttpResponseCode code);
bool SDKShouldRetry(QsError sdkErr, QingStor::Http::HttpResponseCode code);
bool SDKResponseSuccess(QsError sdkErr, QingStor::Http::HttpResponseCode code);

std::string SDKResponseCodeToName(QingStor::Http::HttpResponseCode code);
int SDKResponseCodeToInt(QingStor::Http::HttpResponseCode code);
std::string SDKResponseCodeToString(QingStor::Http::HttpResponseCode code);

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_QSERROR_H_
