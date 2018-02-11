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

#ifndef QSFS_CLIENT_CONSTANTS_H_
#define QSFS_CLIENT_CONSTANTS_H_

#include <stdint.h>  // for uint16_t

namespace QS {

namespace Client {

namespace Constants {

// For a better performance we set a lower value than the limit of 1000
// for per transaction of ListObject to lower than max value.
// https://docs.qingcloud.com/qingstor/api/bucket/get.html
// default value is 200, but test shows 500 give better performance
static const uint16_t BucketListObjectsLimit = 500;

// limitation for per tranasction of DeleteMulitipleObjects
// https://docs.qingcloud.com/qingstor/api/bucket/delete_multiple.html
static const uint16_t BucketDeleteMultipleObjectsLimit = 200;

}  // namespace Constants
}  // namespace Client
}  // namespace QS


#endif  // QSFS_CLIENT_CONSTANTS_H_
