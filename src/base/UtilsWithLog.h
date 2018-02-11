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

#ifndef QSFS_BASE_UTILSWITHLOG_H_
#define QSFS_BASE_UTILSWITHLOG_H_

#include <stdint.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <utility>

namespace QS {

namespace UtilsWithLog {

// Create directory recursively if it doesn't exists
//
// @param  : dir path
// @return : bool
bool CreateDirectoryIfNotExists(const std::string &path);

// Remove directory if it exists
//
// @param  : dir path
// @return : bool
bool RemoveDirectoryIfExists(const std::string &path);

// Remove file if it exists
//
// @param  : file path
// @return : bool
bool RemoveFileIfExists(const std::string &path);

// Delete files in dir recursively
//
// @param  : dir path, flag to delete dir itself
// @return : bool
//
// Print error message if fail to delete
bool DeleteFilesInDirectory(const std::string &path, bool deleteDirectorySelf);

// Check if file exists
bool FileExists(const std::string &path);

// Check if file is a directory
bool IsDirectory(const std::string &path);

// Check if dir is empty
//
// @param  : dir path
// @return : bool
bool IsDirectoryEmpty(const std::string &dir);

// Get user name of uid
//
// @param  : uid, log on flag
// @return : user name
std::string GetUserName(uid_t uid);

// Check if given uid is included in group of gid
//
// @param  : uid, gid, log on flag
// @return : bool
bool IsIncludedInGroup(uid_t uid, gid_t gid);

// Check if process has access permission to the file
//
// @param  : file stat, log on flag
// @return : bool
bool HavePermission(const std::string &path);
bool HavePermission(struct stat *st);

// Get the disk free space
//
// @param  : absolute path
// @return : uint64_t
uint64_t GetFreeDiskSpace(const std::string &absolutePath);

// Check if disk has available free space
//
// @param  : absolute path, free space needed
// @return : bool
bool IsSafeDiskSpace(const std::string &absolutePath, uint64_t freeSpace);

}  // namespace UtilsWithLog
}  // namespace QS

#endif  // QSFS_BASE_UTILSWITHLOG_H_
