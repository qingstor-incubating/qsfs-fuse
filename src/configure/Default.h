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

#ifndef QSFS_CONFIGURE_DEFAULT_H_
#define QSFS_CONFIGURE_DEFAULT_H_

#include <stddef.h>
#include <stdint.h>  // for fixed width integer types

#include <sys/types.h>  // for blkcnt_t

#include <string>
#include <vector>

namespace QS {

namespace Configure {

namespace Default {

const char* GetProgramName();

std::string GetDefaultCredentialsFile();
std::string GetDefaultDiskCacheDirectory();
std::string GetDefaultLogDirectory();
std::string GetDefaultLogLevelName();
std::string GetDefaultHostName();
uint16_t GetDefaultPort(const std::string& protocolName);
std::string GetDefaultProtocolName();
std::string GetDefaultZone();
std::vector<std::string> GetMimeFiles();

uint16_t GetPathMaxLen();
uint16_t GetNameMaxLen();

mode_t GetRootMode();
mode_t GetDefineFileMode();
mode_t GetDefineDirMode();

uint16_t GetBlockSize();  // Block size for filesystem I/O
uint16_t GetFragmentSize();
blkcnt_t GetBlocks(off_t size);  // Number of 512B blocks allocated

uint64_t GetMaxCacheSize();         // File data cache size in bytes
size_t GetMaxStatCount();           // File meta data cache max count
uint16_t GetMaxListObjectsCount();  // max count for list operation

uint16_t GetDefaultTransactionRetries();
uint32_t GetDefaultTransactionTimeDuration();  // in milliseconds
int GetClientDefaultPoolSize();
const char* GetSDKLogFolderBaseName();

size_t GetDefaultParallelTransfers();
uint64_t GetDefaultTransferMaxBufHeapSize();
uint64_t GetDefaultTransferBufSize();

uint64_t GetUploadMultipartMinPartSize();
uint64_t GetUploadMultipartMaxPartSize();
uint64_t GetUploadMultipartThresholdSize();

}  // namespace Default
}  // namespace Configure
}  // namespace QS

// NOLINTNEXTLIN
#endif  // QSFS_CONFIGURE_DEFAULT_H_
