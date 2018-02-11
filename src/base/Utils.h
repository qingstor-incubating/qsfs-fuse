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

#ifndef QSFS_BASE_UTILS_H_
#define QSFS_BASE_UTILS_H_

#include <stdint.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <utility>

namespace QS {

namespace Utils {

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
// @return : a pair of {true,""} or {false, message}
//
std::pair<bool, std::string> DeleteFilesInDirectory(const std::string &path,
                                                    bool deleteDirectorySelf);

// Check if file exists
bool FileExists(const std::string &path);

// Check if file is a directory
std::pair<bool, std::string> IsDirectory(const std::string &path);

// Check if dir is empty
//
// @param  : dir path
// @return : bool
std::pair<bool, std::string> IsDirectoryEmpty(const std::string &dir);

// Get user name of uid
//
// @param  : uid, log on flag
// @return : user name
std::pair<std::string, std::string> GetUserName(uid_t uid);

// Check if given uid is included in group of gid
//
// @param  : uid, gid, log on flag
// @return : bool
std::pair<bool, std::string> IsIncludedInGroup(uid_t uid, gid_t gid);

// Check if process has access permission to the file
//
// @param  : file stat, log on flag
// @return : bool
std::pair<bool, std::string> HavePermission(const std::string &path);
std::pair<bool, std::string> HavePermission(struct stat *st);

// Get the disk free space
//
// @param  : absolute path
// @return : uint64_t, error msg
std::pair<uint64_t, std::string> GetFreeDiskSpace(
    const std::string &absolutePath);

// Check if disk has available free space
//
// @param  : absolute path, free space needed
// @return : bool, error msg
std::pair<bool, std::string> IsSafeDiskSpace(const std::string &absolutePath,
                                             uint64_t freeSpace);

// Check if path is root
bool IsRootDirectory(const std::string &path);

// Append delim to path
//
// @param  : file path
// @return : path appended
std::string AppendPathDelim(const std::string &path);

// Get path delimiter
std::string GetPathDelimiter();

// Get dir name where the file belongs to
//
// @param  : file path
// @return : dir name ending with "/"
//
// If path is root or cannot find dir, return null string
std::string GetDirName(const std::string &path);

// Get file name from file path
//
// @param  : file path
// @return : file name
//
// If path is root or cannot find base name, return null string
std::string GetBaseName(const std::string &path);

// Get parent dir of file
//
// @param  : file path
// @return : a pair of {true, parent dir} or {false, message}
//
// At beginning will check if the file exists, if not return false.
// If file exists, return the file dir name.
std::pair<bool, std::string> GetParentDirectory(const std::string &path);

// Get calling process effective user id
//
// @param  : void
// @return : uid
uid_t GetProcessEffectiveUserID();

// Get calling process effective group id
//
// @param  : void
// @return : gid
gid_t GetProcessEffectiveGroupID();

}  // namespace Utils
}  // namespace QS

#endif  // QSFS_BASE_UTILS_H_
