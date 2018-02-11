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

#include "base/UtilsWithLog.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>  // for strerror

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <utility>

#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/Utils.h"

namespace QS {

namespace UtilsWithLog {

using QS::StringUtils::FormatPath;
using std::pair;
using std::string;

static const char PATH_DELIM = '/';

namespace {

string PostErrMsg(const string &path) {
  return string(": ") + strerror(errno) + " " + FormatPath(path);
}

}  // namespace

// --------------------------------------------------------------------------
bool CreateDirectoryIfNotExists(const string &path) {
  if (FileExists(path)) {
    return IsDirectory(path);  // do nothing
  } else {
    bool success = QS::Utils::CreateDirectoryIfNotExists(path);
    if (success) {
      Info("Create directory " + FormatPath(path));
    } else {
      DebugWarning("Fail to create directory " + PostErrMsg(path));
    }
    return success;
  }
}

// --------------------------------------------------------------------------
bool RemoveDirectoryIfExists(const string &path) {
  if (FileExists(path)) {
    bool success = QS::Utils::RemoveDirectoryIfExists(path);
    if (success) {
      Info("Delete directory " + FormatPath(path));
    } else {
      DebugWarning("Fail to delete directory " + PostErrMsg(path));
    }
    return success;
  } else {
    return true;  // do nothing
  }
}

// --------------------------------------------------------------------------
bool RemoveFileIfExists(const string &path) {
  if (FileExists(path)) {
    bool success = QS::Utils::RemoveFileIfExists(path);
    if (success) {
      Info("Remove file " + FormatPath(path));
    } else {
      DebugWarning("Fail to delete file " + PostErrMsg(path));
    }
    return success;
  } else {
    return true;  // do nothing
  }
}

// --------------------------------------------------------------------------
bool DeleteFilesInDirectory(const std::string &path, bool deleteSelf) {
  pair<bool, string> outcome =
      QS::Utils::DeleteFilesInDirectory(path, deleteSelf);
  DebugWarningIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool FileExists(const string &path) {
  if (QS::Utils::FileExists(path)) {
    return true;
  } else {
    DebugInfo("File not exists " + PostErrMsg(path));
    return false;
  }
}

// --------------------------------------------------------------------------
bool IsDirectory(const string &path) {
  pair<bool, string> outcome = QS::Utils::IsDirectory(path);
  DebugInfoIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool IsDirectoryEmpty(const std::string &path) {
  pair<bool, string> outcome = QS::Utils::IsDirectoryEmpty(path);
  DebugInfoIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
string GetUserName(uid_t uid) {
  pair<string, string> outcome = QS::Utils::GetUserName(uid);
  DebugInfoIf(outcome.first.empty() && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool IsIncludedInGroup(uid_t uid, gid_t gid) {
  pair<bool, string> outcome = QS::Utils::IsIncludedInGroup(uid, gid);
  DebugInfoIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool HavePermission(const std::string &path) {
  pair<bool, string> outcome = QS::Utils::HavePermission(path);
  DebugInfoIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool HavePermission(struct stat *st) {
  pair<bool, string> outcome = QS::Utils::HavePermission(st);
  DebugInfoIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
uint64_t GetFreeDiskSpace(const string &absolutePath) {
  pair<uint64_t, string> outcome = QS::Utils::GetFreeDiskSpace(absolutePath);
  DebugWarningIf(outcome.first == 0 && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

// --------------------------------------------------------------------------
bool IsSafeDiskSpace(const string &absolutePath, uint64_t freeSpace) {
  pair<bool, string> outcome =
      QS::Utils::IsSafeDiskSpace(absolutePath, freeSpace);
  DebugWarningIf(!outcome.first && !outcome.second.empty(), outcome.second);
  return outcome.first;
}

}  // namespace UtilsWithLog
}  // namespace QS
