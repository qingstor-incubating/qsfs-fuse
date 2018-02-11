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

#include "base/Utils.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>  // for strerror

#include <dirent.h>  // for opendir readdir
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>  // for access

#include <string>
#include <utility>
#include <vector>

#include "boost/exception/to_string.hpp"
#include "boost/scope_exit.hpp"

#include "base/StringUtils.h"
#include "configure/Default.h"

namespace QS {

namespace Utils {

using boost::to_string;
using QS::StringUtils::FormatPath;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

static const char PATH_DELIM = '/';

namespace {

string PostErrMsg(const string &path) {
  return string(": ") + strerror(errno) + " " + FormatPath(path);
}

}  // namespace

// --------------------------------------------------------------------------
bool CreateDirectoryIfNotExists(const string &path) {
  if (path.empty()) {
    return false;
  }
  if (IsRootDirectory(path)) {
    return true;
  }
  if (FileExists(path)) {
    return IsDirectory(path).first;
  } else {
    // if parent dir exist or created
    if (CreateDirectoryIfNotExists(GetDirName(path))) {
      int errorCode =
          mkdir(path.c_str(), QS::Configure::Default::GetDefineDirMode());
      bool success = (errorCode == 0 || errno == EEXIST);
      return success;
    } else {
      return false;
    }
  }
}

// --------------------------------------------------------------------------
bool RemoveDirectoryIfExists(const string &path) {
  int errorCode = rmdir(path.c_str());
  return (errorCode == 0 || errno == ENOENT || errno == ENOTDIR);
}

// --------------------------------------------------------------------------
bool RemoveFileIfExists(const string &path) {
  int errorCode = unlink(path.c_str());
  return (errorCode == 0 || errno == ENOENT);
}

// --------------------------------------------------------------------------
pair<bool, string> DeleteFilesInDirectory(const std::string &path,
                                          bool deleteSelf) {
  bool success = true;
  string msg;

  DIR *dir = opendir(path.c_str());
  BOOST_SCOPE_EXIT((dir)) {
    if (dir) {
      closedir(dir);
      dir = NULL;
    }
  }
  BOOST_SCOPE_EXIT_END

  if (dir) {
    struct dirent *nextDir = NULL;
    while ((nextDir = readdir(dir)) != NULL) {
      if (strcmp(nextDir->d_name, ".") == 0 ||
          strcmp(nextDir->d_name, "..") == 0) {
        continue;
      }

      string fullPath(path);
      fullPath.append(1, PATH_DELIM);
      fullPath.append(nextDir->d_name);

      struct stat st;
      if (lstat(fullPath.c_str(), &st) != 0) {
        success = false;
        msg.assign("Could not get stats of file " + PostErrMsg(fullPath));
        break;
      }

      if (S_ISDIR(st.st_mode)) {
        if (!DeleteFilesInDirectory(fullPath, true).first) {
          success = false;
          msg.assign("Could not remove subdirectory " + PostErrMsg(fullPath));
          break;
        }
      } else {
        if (unlink(fullPath.c_str()) != 0) {
          success = false;
          msg.assign("Could not remove file " + PostErrMsg(fullPath));
          break;
        }
      }  // end of S_ISDIR
    }    // end of while
  } else {
    success = false;
    msg.assign("Could not open directory " + PostErrMsg(path));
  }

  if (deleteSelf && rmdir(path.c_str()) != 0) {
    success = false;
    msg.assign("Could not remove dir " + PostErrMsg(path));
  }

  return make_pair(success, msg);
}

// --------------------------------------------------------------------------
bool FileExists(const string &path) {
  int errorCode = access(path.c_str(), F_OK);
  return errorCode == 0;
}

// --------------------------------------------------------------------------
pair<bool, string> IsDirectory(const string &path) {
  bool success = true;
  string msg;

  struct stat stBuf;
  if (stat(path.c_str(), &stBuf) != 0) {
    msg.assign("Unable to access path " + PostErrMsg(path));
    success = false;
  } else {
    success = S_ISDIR(stBuf.st_mode);
  }

  return make_pair(success, msg);
}

// --------------------------------------------------------------------------
pair<bool, string> IsDirectoryEmpty(const std::string &path) {
  bool success = true;
  string msg;

  DIR *dir = opendir(path.c_str());
  BOOST_SCOPE_EXIT((dir)) {
    if (dir) {
      closedir(dir);
      dir = NULL;
    }
  }
  BOOST_SCOPE_EXIT_END

  if (dir) {
    struct dirent *nextDir = NULL;
    while ((nextDir = readdir(dir)) != NULL) {
      if (strcmp(nextDir->d_name, ".") != 0 &&
          strcmp(nextDir->d_name, "..") != 0) {
        success = false;
      }
    }
  } else {
    success = false;
    msg.assign("Failed to open path " + PostErrMsg(path));
  }

  return make_pair(success, msg);
}

// --------------------------------------------------------------------------
pair<string, string> GetUserName(uid_t uid) {
  string userName;
  string msg;

  int32_t maxBufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
  assert(maxBufSize > 0);
  if (!(maxBufSize > 0)) {
    msg.assign("Fail to get maximum size of getpwuid_r() data buffer");
    return make_pair(userName, msg);
  }

  vector<char> buffer(maxBufSize);
  struct passwd pwdInfo;
  struct passwd *result = NULL;
  if (getpwuid_r(uid, &pwdInfo, &buffer[0], maxBufSize, &result) != 0) {
    msg.assign(string("Fail to get passwd information : ") + strerror(errno));
    return make_pair(userName, msg);
  }

  if (result == NULL) {
    msg.assign("No data in passwd [uid= " + to_string(uid) + "]");
    return make_pair(userName, msg);
  }

  userName.assign(result->pw_name);
  if (userName.empty()) {
    msg.assign("Empty username of uid " + to_string(uid));
  }
  return make_pair(userName, msg);
}

// --------------------------------------------------------------------------
pair<bool, string> IsIncludedInGroup(uid_t uid, gid_t gid) {
  bool success = true;
  string msg;

  int32_t maxBufSize = sysconf(_SC_GETGR_R_SIZE_MAX);
  assert(maxBufSize > 0);
  if (!(maxBufSize > 0)) {
    msg.assign("Fail to get maximum size of getgrgid_r() data buffer");
    success = false;
    return make_pair(success, msg);
  }

  vector<char> buffer(maxBufSize);
  struct group grpInfo;
  struct group *result = NULL;
  if (getgrgid_r(gid, &grpInfo, &buffer[0], maxBufSize, &result) != 0) {
    msg.assign(string("Fail to get group information : ") + strerror(errno));
    success = false;
    return make_pair(success, msg);
  }
  if (result == NULL) {
    msg.assign("No gid in group [gid=" + to_string(gid) + "]");
    success = false;
    return make_pair(success, msg);
  }

  pair<string, string> outcome = GetUserName(uid);
  string userName = outcome.first;
  if (userName.empty()) {
    msg.assign(outcome.second);
    success = false;
    return make_pair(success, msg);
  }

  char **groupMember = result->gr_mem;
  while (groupMember && *groupMember) {
    if (userName == *groupMember) {
      success = true;
      return make_pair(success, msg);
    }
    ++groupMember;
  }

  return make_pair(success, msg);
}

// --------------------------------------------------------------------------
pair<bool, string> HavePermission(const std::string &path) {
  struct stat st;
  int errorCode = stat(path.c_str(), &st);
  if (errorCode != 0) {
    string msg = "Unable to access file when trying to check its permission " +
                 PostErrMsg(path);
    return make_pair(false, msg);
  } else {
    return HavePermission(&st);
  }
}

// --------------------------------------------------------------------------
pair<bool, string> HavePermission(struct stat *st) {
  string msg;
  // Check type
  if (st == NULL) {
    return make_pair(false, "Null stat input");
  }

  uid_t uidProcess = GetProcessEffectiveUserID();
  gid_t gidProcess = GetProcessEffectiveGroupID();

  // Check owner
  if (0 == uidProcess || st->st_uid == uidProcess) {
    return make_pair(true, msg);
  }

  // Check group
  if (st->st_gid == gidProcess ||
      IsIncludedInGroup(uidProcess, st->st_gid).first) {
    if (S_IRWXG == (st->st_mode & S_IRWXG)) {
      return make_pair(true, msg);
    }
  }

  // Check others
  if (S_IRWXO == (st->st_mode & S_IRWXO)) {
    return make_pair(true, msg);
  }

  msg.assign("No permission, [Process uid:gid=" + to_string(uidProcess) + ":" +
             to_string(gidProcess) + ", File uid:gid=" + to_string(st->st_uid) +
             ":" + to_string(st->st_gid) + "]");
  return make_pair(false, msg);
}

// --------------------------------------------------------------------------
pair<uint64_t, string> GetFreeDiskSpace(const string &absolutePath) {
  struct statvfs vfsbuf;
  int ret = statvfs(absolutePath.c_str(), &vfsbuf);
  if (ret != 0) {
    return make_pair(0,
                     "Fail to get free disk space " + PostErrMsg(absolutePath));
  }
  return make_pair(vfsbuf.f_bavail * vfsbuf.f_bsize, string());
}

// --------------------------------------------------------------------------
pair<bool, string> IsSafeDiskSpace(const string &absolutePath,
                                   uint64_t freeSpace) {
  pair<uint64_t, string> outcome = GetFreeDiskSpace(absolutePath);
  uint64_t totalFreeSpace = outcome.first;
  return make_pair(totalFreeSpace > freeSpace, outcome.second);
}

// --------------------------------------------------------------------------
bool IsRootDirectory(const std::string &path) { return path == "/"; }

// --------------------------------------------------------------------------
string AppendPathDelim(const string &path) {
  assert(!path.empty());
  string cpy(path);
  if (path[path.size() - 1] != PATH_DELIM) {
    cpy.append(1, PATH_DELIM);
  }
  return cpy;
}

// --------------------------------------------------------------------------
string GetPathDelimiter() { return string(1, PATH_DELIM); }

// --------------------------------------------------------------------------
std::string GetDirName(const std::string &path) {
  if (IsRootDirectory(path)) {
    return path;
  }

  char *cpy = strdup(path.c_str());
  string ret = AppendPathDelim(dirname(cpy));
  free(cpy);
  return ret;
}

// --------------------------------------------------------------------------
std::string GetBaseName(const std::string &path) {
  char *cpy = strdup(path.c_str());
  string ret(basename(cpy));
  free(cpy);
  return ret;
}

// --------------------------------------------------------------------------
pair<bool, string> GetParentDirectory(const string &path) {
  bool success = false;
  string str;
  if (FileExists(path)) {
    if (IsRootDirectory(path)) {
      success = true;
      str.assign("/");  // return root
    } else {
      str = path;
      if (str[str.size() - 1] == PATH_DELIM) {
        str.erase(str.size() - 1, 1);
      }
      string::size_type pos = str.find_last_of(PATH_DELIM);
      if (pos != string::npos) {
        success = true;
        str = str.substr(0, pos + 1);  // including the ending "/"
      } else {
        str.assign("Unable to find parent directory " + FormatPath(path));
      }
    }
  } else {
    str.assign("Unable to access " + FormatPath(path));
  }

  return make_pair(success, str);
}

// --------------------------------------------------------------------------
uid_t GetProcessEffectiveUserID() {
  static uid_t uid = geteuid();
  return uid;
}

// --------------------------------------------------------------------------
gid_t GetProcessEffectiveGroupID() {
  static gid_t gid = getegid();
  return gid;
}

}  // namespace Utils
}  // namespace QS
