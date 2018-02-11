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

#include "base/StringUtils.h"

#include <sys/stat.h>  // for S_ISUID
#include <unistd.h>    // for R_OK

#include <algorithm>
#include <cctype>
#include <string>

#include "boost/foreach.hpp"
#include "boost/lambda/lambda.hpp"

namespace QS {

namespace StringUtils {

using std::string;

// --------------------------------------------------------------------------
string ToLower(const string &str) {
  string copy(str);
  BOOST_FOREACH(char &ch, copy) { ch = std::tolower(ch); }
  return copy;
}

// --------------------------------------------------------------------------
string ToUpper(const string &str) {
  string copy(str);
  BOOST_FOREACH(char &ch, copy) { ch = std::toupper(ch); }
  return copy;
}

// --------------------------------------------------------------------------
string LTrim(const string &str, unsigned char ch) {
  using boost::lambda::_1;
  string copy(str);
  string::iterator pos = std::find_if(copy.begin(), copy.end(), ch != _1);
  copy.erase(copy.begin(), pos);
  return copy;
}

// --------------------------------------------------------------------------
string RTrim(const string &str, unsigned char ch) {
  using boost::lambda::_1;
  string copy(str);
  string::reverse_iterator rpos =
      std::find_if(copy.rbegin(), copy.rend(), ch != _1);
  copy.erase(rpos.base(), copy.end());
  return copy;
}

// --------------------------------------------------------------------------
string Trim(const string &str, unsigned char ch) {
  return LTrim(RTrim(str, ch), ch);
}

// --------------------------------------------------------------------------
string AccessMaskToString(int amode) {
  string ret;
  if (amode & R_OK) {
    ret.append("R_OK ");
  }
  if (amode & W_OK) {
    ret.append("W_OK ");
  }
  if (amode & X_OK) {
    ret.append("X_OK");
  }
  ret = QS::StringUtils::RTrim(ret, ' ');
  string::size_type pos = 0;
  while ((pos = ret.find(' ')) != string::npos) {
    ret.replace(pos, 1, "|");
  }
  return ret;
}

// --------------------------------------------------------------------------
std::string ModeToString(mode_t mode) {
  string modeStr;
  modeStr.append(1, GetFileTypeLetter(mode));

  // access MODE bits          000    001    010    011
  //                           100    101    110    111
  static const char *rwx[] = {"---", "--x", "-w-", "-wx",
                              "r--", "r-x", "rw-", "rwx"};

  modeStr.append(rwx[(mode >> 6) & 7]);  // user
  modeStr.append(rwx[(mode >> 3) & 7]);  // group
  modeStr.append(rwx[(mode & 7)]);

  if (mode & S_ISUID) {
    modeStr[3] = (mode & S_IXUSR) ? 's' : 'S';
  }
  if (mode & S_ISGID) {
    modeStr[6] = (mode & S_IXGRP) ? 's' : 'l';
  }
  if (mode & S_ISVTX) {
    modeStr[9] = (mode & S_IXUSR) ? 't' : 'T';
  }
  return modeStr;
}

// --------------------------------------------------------------------------
char GetFileTypeLetter(mode_t mode) {
  char c = '?';

  if (S_ISREG(mode)) {
    c = '-';
  } else if (S_ISDIR(mode)) {
    c = 'd';
  } else if (S_ISBLK(mode)) {
    c = 'b';
  } else if (S_ISCHR(mode)) {
    c = 'c';
  }
#ifdef S_ISFIFO
  else if (S_ISFIFO(mode)) {  // NOLINT
    c = 'p';
  }
#endif /* S_ISFIFO */
#ifdef S_ISLNK
  else if (S_ISLNK(mode)) {  // NOLINT
    c = 'l';
  }
#endif /* S_ISLNK */
#ifdef S_ISSOCK
  else if (S_ISSOCK(mode)) {  // NOLINT
    c = 's';
  }
#endif /* S_ISSOCK */
#ifdef S_ISDOOR
  /* Solaris 2.6, etc. */
  else if (S_ISDOOR(mode)) {  // NOLINT
    c = 'D';
  }
#endif    /* S_ISDOOR */
  else {  // NOLINT
    /* Unknown type -- possibly a regular file? */
    c = '?';
  }

  return (c);
}

// --------------------------------------------------------------------------
string FormatPath(const string &path) { return "[path=" + path + "]"; }

// --------------------------------------------------------------------------
string FormatPath(const string &from, const string &to) {
  return "[from=" + from + " to=" + to + "]";
}

}  // namespace StringUtils
}  // namespace QS
