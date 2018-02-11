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

#ifndef QSFS_CLIENT_QSCLIENTOUTCOME_H_
#define QSFS_CLIENT_QSCLIENTOUTCOME_H_

#include <vector>

#include "qingstor/Bucket.h"

#include "client/Outcome.hpp"
#include "client/QSError.h"

namespace QS {

namespace Client {

typedef Outcome<QingStor::GetBucketStatisticsOutput,
                ClientError<QSError::Value> >
    GetBucketStatisticsOutcome;
typedef Outcome<QingStor::HeadBucketOutput, ClientError<QSError::Value> >
    HeadBucketOutcome;
typedef Outcome<std::vector<QingStor::ListObjectsOutput>,
                ClientError<QSError::Value> >
    ListObjectsOutcome;
typedef Outcome<QingStor::DeleteMultipleObjectsOutput,
                ClientError<QSError::Value> >
    DeleteMultipleObjectsOutcome;
typedef Outcome<std::vector<QingStor::ListMultipartUploadsOutput>,
                ClientError<QSError::Value> >
    ListMultipartUploadsOutcome;

typedef Outcome<QingStor::DeleteObjectOutput, ClientError<QSError::Value> >
    DeleteObjectOutcome;
typedef Outcome<QingStor::GetObjectOutput, ClientError<QSError::Value> >
    GetObjectOutcome;
typedef Outcome<QingStor::HeadObjectOutput, ClientError<QSError::Value> >
    HeadObjectOutcome;
typedef Outcome<QingStor::PutObjectOutput, ClientError<QSError::Value> >
    PutObjectOutcome;

typedef Outcome<QingStor::InitiateMultipartUploadOutput,
                ClientError<QSError::Value> >
    InitiateMultipartUploadOutcome;
typedef Outcome<QingStor::UploadMultipartOutput, ClientError<QSError::Value> >
    UploadMultipartOutcome;
typedef Outcome<QingStor::CompleteMultipartUploadOutput,
                ClientError<QSError::Value> >
    CompleteMultipartUploadOutcome;
typedef Outcome<QingStor::AbortMultipartUploadOutput,
                ClientError<QSError::Value> >
    AbortMultipartUploadOutcome;
typedef Outcome<std::vector<QingStor::ListMultipartOutput>,
                ClientError<QSError::Value> >
    ListMultipartOutcome;

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_QSCLIENTOUTCOME_H_
