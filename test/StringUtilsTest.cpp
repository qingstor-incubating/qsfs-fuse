// +-------------------------------------------------------------------------
// | Copyright (C) 2018 Yunify, Inc.
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

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "gtest/gtest.h"

#include "base/StringUtils.h"

using std::string;

TEST(StringUtilsTest, ChangeCase) {
  string lowercase = "lowercase";
  EXPECT_EQ(lowercase, QS::StringUtils::ToLower("LOWerCase"));

  string uppercase = "UPPERCASE";
  EXPECT_EQ(uppercase, QS::StringUtils::ToUpper("UpperCase"));
}

TEST(StringUtilsTest, Trim) {
  string raw = "    hello world    ";
  string notrailing = "    hello world";
  string noleading = "hello world    ";
  string noboth = "hello world";
  char ch = ' ';

  EXPECT_EQ(notrailing, QS::StringUtils::RTrim(raw, ch));
  EXPECT_EQ(noleading, QS::StringUtils::LTrim(raw, ch));
  EXPECT_EQ(noboth, QS::StringUtils::Trim(raw, ch));
}

TEST(StringUtilsTest, FileMode) {
  using QS::StringUtils::AccessMaskToString;
  EXPECT_EQ(string("R_OK"), AccessMaskToString(R_OK));
  EXPECT_EQ(string("W_OK"), AccessMaskToString(W_OK));
  EXPECT_EQ(string("X_OK"), AccessMaskToString(X_OK));
  EXPECT_EQ(string("R_OK|W_OK"), AccessMaskToString(R_OK | W_OK));
  EXPECT_EQ(string("R_OK|W_OK|X_OK"), AccessMaskToString(R_OK | W_OK | X_OK));
}

TEST(StringUtilsTest, FilePermission) {
  using QS::StringUtils::ModeToString;
  EXPECT_EQ(string("?r--------"), ModeToString(S_IRUSR));
  EXPECT_EQ(string("?-w-------"), ModeToString(S_IWUSR));
  EXPECT_EQ(string("?--x------"), ModeToString(S_IXUSR));
  EXPECT_EQ(string("?rwx------"), ModeToString(S_IRWXU));
  EXPECT_EQ(string("?---r-----"), ModeToString(S_IRGRP));
  EXPECT_EQ(string("?----w----"), ModeToString(S_IWGRP));
  EXPECT_EQ(string("?-----x---"), ModeToString(S_IXGRP));
  EXPECT_EQ(string("?---rwx---"), ModeToString(S_IRWXG));
  EXPECT_EQ(string("?------r--"), ModeToString(S_IROTH));
  EXPECT_EQ(string("?-------w-"), ModeToString(S_IWOTH));
  EXPECT_EQ(string("?--------x"), ModeToString(S_IXOTH));
  EXPECT_EQ(string("?------rwx"), ModeToString(S_IRWXO));
  EXPECT_EQ(string("?rwxrwx---"), ModeToString(S_IRWXU | S_IRWXG));
  EXPECT_EQ(string("?rwx---rwx"), ModeToString(S_IRWXU | S_IRWXO));
  EXPECT_EQ(string("?---rwxrwx"), ModeToString(S_IRWXG | S_IRWXO));
  EXPECT_EQ(string("?rwxrwxrwx"), ModeToString(S_IRWXU | S_IRWXG | S_IRWXO));
}

TEST(StringUtilsTest, FileType) {
  using QS::StringUtils::GetFileTypeLetter;
  EXPECT_EQ('-', GetFileTypeLetter(S_IFREG));
  EXPECT_EQ('d', GetFileTypeLetter(S_IFDIR));
  EXPECT_EQ('b', GetFileTypeLetter(S_IFBLK));
  EXPECT_EQ('c', GetFileTypeLetter(S_IFCHR));
  EXPECT_EQ('p', GetFileTypeLetter(S_IFIFO));
  EXPECT_EQ('l', GetFileTypeLetter(S_IFLNK));
  EXPECT_EQ('s', GetFileTypeLetter(S_IFSOCK));
  EXPECT_EQ('?', GetFileTypeLetter(S_IFREG - 1));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
