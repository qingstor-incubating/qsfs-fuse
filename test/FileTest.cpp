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

#include <string.h>

#include <list>
#include <sstream>
#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "boost/array.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/tuple/tuple.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "configure/Options.h"
#include "data/File.h"
#include "data/Page.h"

namespace QS {

namespace Data {

using boost::array;
using boost::make_shared;
using boost::shared_ptr;
using boost::tuple;
using std::list;
using std::make_pair;
using std::pair;
using std::string;
using std::stringstream;
using ::testing::Test;

// default log dir
static const char *defaultLogDir = "/tmp/qsfs.test.logs/";
void InitLog() {
  QS::Utils::CreateDirectoryIfNotExists(defaultLogDir);
  QS::Logging::Log::Instance().Initialize(defaultLogDir);
}

time_t mtime_ = time(NULL);
uid_t uid_ = 1000U;
gid_t gid_ = 1000U;
mode_t fileMode_ = S_IRWXU | S_IRWXG | S_IROTH;

class FileTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  void TestWrite() {
    string filename = "file1";
    File file1(filename, mtime_);  // empty file

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.Write(off1, len1, page1, 0);
    EXPECT_EQ(file1.GetSize(), len1);
    EXPECT_EQ(file1.GetCachedSize(), len1);
    EXPECT_FALSE(file1.UseDiskFile());
    EXPECT_TRUE(file1.HasData(0, len1 - 1));
    EXPECT_TRUE(file1.HasData(0, len1));
    EXPECT_FALSE(file1.HasData(0, len1 + 1));
    EXPECT_TRUE(file1.GetUnloadedRanges(0, len1).empty());
    EXPECT_FALSE(file1.GetUnloadedRanges(0, len1 + 1).empty());
    EXPECT_EQ(file1.GetNumPages(), 1u);

    const char *data = "abc";
    size_t len2 = 3;
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    file1.Write(off2, len2, page2, 0);
    EXPECT_EQ(file1.GetSize(), len1 + len2);
    EXPECT_EQ(file1.GetCachedSize(), len1 + len2);
    EXPECT_TRUE(file1.HasData(0, len1 + len2 - 1));
    EXPECT_TRUE(file1.HasData(0, len1 + len2));
    EXPECT_FALSE(file1.HasData(0, len1 + len2 + 1));
    EXPECT_TRUE(file1.GetUnloadedRanges(0, len1 + len2).empty());
    EXPECT_FALSE(file1.GetUnloadedRanges(0, len1 + len2 + 1).empty());
    EXPECT_EQ(file1.GetNumPages(), 2u);

    pair<PageSetConstIterator, PageSetConstIterator> consecutivePages =
        file1.ConsecutivePageRangeAtFront();
    EXPECT_TRUE(consecutivePages.first == file1.BeginPage());
    EXPECT_TRUE(consecutivePages.second == file1.EndPage());

    array<char, 3> arr1;
    arr1[0] = '0';
    arr1[1] = '1';
    arr1[2] = '2';
    array<char, 3> buf1;
    file1.Front()->Read(&buf1[0]);
    EXPECT_EQ(buf1, arr1);

    array<char, 3> arr2;
    arr2[0] = 'a';
    arr2[1] = 'b';
    arr2[2] = 'c';
    array<char, 3> buf2;
    file1.Back()->Read(&buf2[0]);
    EXPECT_EQ(buf2, arr2);

    EXPECT_TRUE(file1.LowerBoundPage(0) == file1.BeginPage());
    EXPECT_TRUE(file1.LowerBoundPage(len1 - 1) == ++file1.BeginPage());
    EXPECT_TRUE(file1.LowerBoundPage(len1) == ++file1.BeginPage());
    EXPECT_TRUE(file1.LowerBoundPage(len1 + 1) == file1.EndPage());
    EXPECT_TRUE(file1.LowerBoundPage(len1 + len2) == file1.EndPage());

    pair<PageSetConstIterator, PageSetConstIterator> range =
        file1.IntesectingRange(0, len1);
    EXPECT_TRUE(range.first == file1.BeginPage());
    EXPECT_TRUE(range.second == ++file1.BeginPage());
    pair<PageSetConstIterator, PageSetConstIterator> range1 =
        file1.IntesectingRange(len1 - 1, len1);
    EXPECT_TRUE(range1.first == file1.BeginPage());
    EXPECT_TRUE(range1.second == ++file1.BeginPage());
    pair<PageSetConstIterator, PageSetConstIterator> range2 =
        file1.IntesectingRange(len1, len1 + 1);
    EXPECT_TRUE(range2.first == ++file1.BeginPage());
    EXPECT_TRUE(range2.second == file1.EndPage());

    const char *page3 = "ABC";
    size_t len3 = 3;
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    file1.Write(off3, len3, page3, 0);
    EXPECT_EQ(file1.GetSize(), len1 + len2 + len3);
    EXPECT_EQ(file1.GetCachedSize(), len1 + len2 + len3);
    EXPECT_TRUE(file1.HasData(off2, len2));
    EXPECT_TRUE(file1.HasData(off3, len3));
    EXPECT_FALSE(file1.HasData(off2 + len3, len3));
    EXPECT_FALSE(file1.HasData(off3 - 1, len3));
    EXPECT_TRUE(file1.GetUnloadedRanges(0, len1 + len2).empty());
    ContentRangeDeque d1;
    d1.push_back(make_pair(len1 + len2, len3));
    EXPECT_EQ(file1.GetUnloadedRanges(0, len1 + len2 + len3), d1);
    ContentRangeDeque d2;
    d2.push_back(make_pair(len1 + len2, holeLen));
    d2.push_back(make_pair(off3 + len3, 1));
    EXPECT_EQ(file1.GetUnloadedRanges(0, off3 + len3 + 1), d2);

    EXPECT_TRUE(file1.LowerBoundPage(len1 + len2 + len3) == --file1.EndPage());
    EXPECT_TRUE(file1.LowerBoundPage(off3) == --file1.EndPage());
    EXPECT_TRUE(file1.LowerBoundPage(off3 + len3) == file1.EndPage());
    EXPECT_TRUE(file1.UpperBoundPage(off2) == --file1.EndPage());
    EXPECT_TRUE(file1.UpperBoundPage(off2 + len2) == --file1.EndPage());
    EXPECT_TRUE(file1.UpperBoundPage(off3) == file1.EndPage());
    pair<PageSetConstIterator, PageSetConstIterator> range3 =
        file1.IntesectingRange(off2 + 1, off3 - 1);
    EXPECT_TRUE(range3.first == ++file1.BeginPage());
    EXPECT_TRUE(range3.second == --file1.EndPage());
    pair<PageSetConstIterator, PageSetConstIterator> range4 =
        file1.IntesectingRange(off3 + 1, off3 + 1);
    EXPECT_TRUE(range4.first == --file1.EndPage());
    EXPECT_TRUE(range4.second == file1.EndPage());
  }

  void TestWriteDiskFile() {
    string filename = "file1";
    File file1(filename, mtime_);  // empty file
    file1.SetUseDiskFile(true);

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.Write(off1, len1, page1, 0);
    EXPECT_EQ(file1.GetSize(), len1);
    EXPECT_EQ(file1.GetCachedSize(), 0u);
    EXPECT_TRUE(file1.UseDiskFile());
    EXPECT_TRUE(file1.HasData(0, len1 - 1));
    EXPECT_TRUE(file1.HasData(0, len1));
    EXPECT_FALSE(file1.HasData(0, len1 + 1));
    EXPECT_TRUE(file1.GetUnloadedRanges(0, len1).empty());
    EXPECT_FALSE(file1.GetUnloadedRanges(0, len1 + 1).empty());
    EXPECT_EQ(file1.GetNumPages(), 1u);

    const char *data = "abc";
    size_t len2 = 3;
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    file1.Write(off2, len2, page2, 0);
    EXPECT_EQ(file1.GetSize(), len1 + len2);
    EXPECT_EQ(file1.GetCachedSize(), 0u);
    EXPECT_TRUE(file1.HasData(0, len1 + len2 - 1));
    EXPECT_TRUE(file1.HasData(0, len1 + len2));
    EXPECT_FALSE(file1.HasData(0, len1 + len2 + 1));
    EXPECT_TRUE(file1.GetUnloadedRanges(0, len1 + len2).empty());
    EXPECT_FALSE(file1.GetUnloadedRanges(0, len1 + len2 + 1).empty());
    EXPECT_EQ(file1.GetNumPages(), 2u);

    pair<PageSetConstIterator, PageSetConstIterator> consecutivePages =
        file1.ConsecutivePageRangeAtFront();
    EXPECT_TRUE(consecutivePages.first == file1.BeginPage());
    EXPECT_TRUE(consecutivePages.second == file1.EndPage());

    array<char, 3> arr1;
    arr1[0] = '0';
    arr1[1] = '1';
    arr1[2] = '2';
    array<char, 3> buf1;
    file1.Front()->Read(&buf1[0]);
    EXPECT_EQ(buf1, arr1);

    array<char, 3> arr2;
    arr2[0] = 'a';
    arr2[1] = 'b';
    arr2[2] = 'c';
    array<char, 3> buf2;
    file1.Back()->Read(&buf2[0]);
    EXPECT_EQ(buf2, arr2);
  }

  void TestRead() {
    string filename = "file1";
    File file1(filename, mtime_);  // empty file

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.Write(off1, len1, page1, mtime_);

    const char *page2 = "abc";
    size_t len2 = 3;
    off_t off2 = off_t(len1);
    file1.Write(off2, len2, page2, mtime_);

    const char *page3 = "ABC";
    size_t len3 = 3;
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    file1.Write(off3, len3, page3, mtime_);

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res1 =
        file1.Read(off1, len1, 0);
    EXPECT_EQ(boost::get<0>(res1), len1);
    list<shared_ptr<Page> > &pages1 = boost::get<1>(res1);
    EXPECT_EQ(pages1.size(), 1u);
    array<char, 3> buf1;
    pages1.front()->Read(&buf1[0]);
    array<char, 3> arr1;
    arr1[0] = '0';
    arr1[1] = '1';
    arr1[2] = '2';
    EXPECT_EQ(buf1, arr1);
    ContentRangeDeque &unloadPages1 = boost::get<2>(res1);
    EXPECT_TRUE(unloadPages1.empty());

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res2 =
        file1.Read(off1 + 1, len1, 0);
    EXPECT_EQ(boost::get<0>(res2), len1 + len2);
    list<shared_ptr<Page> > &pages2 = boost::get<1>(res2);
    EXPECT_EQ(pages2.size(), 2u);
    array<char, 3> buf2;
    pages2.front()->Read(&buf2[0]);
    EXPECT_EQ(buf2, arr1);
    array<char, 3> buf3;
    pages2.back()->Read(&buf3[0]);
    array<char, 3> arr2;
    arr2[0] = 'a';
    arr2[1] = 'b';
    arr2[2] = 'c';
    EXPECT_EQ(buf3, arr2);
    ContentRangeDeque &unloadPages2 = boost::get<2>(res2);
    EXPECT_TRUE(unloadPages2.empty());

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res3 =
        file1.Read(off2 + len2, holeLen, 0);
    EXPECT_EQ(boost::get<0>(res3), 0u);
    list<shared_ptr<Page> > &pages3 = boost::get<1>(res3);
    EXPECT_TRUE(pages3.empty());
    ContentRangeDeque &unloadPages3 = boost::get<2>(res3);
    EXPECT_FALSE(unloadPages3.empty());

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res4 =
        file1.Read(off3, len3, 0);
    EXPECT_EQ(boost::get<0>(res4), len3);
    list<shared_ptr<Page> > &pages4 = boost::get<1>(res4);
    EXPECT_EQ(pages4.size(), 1u);
    array<char, 3> buf4;
    pages4.front()->Read(&buf4[0]);
    array<char, 3> arr3;
    arr3[0] = 'A';
    arr3[1] = 'B';
    arr3[2] = 'C';
    EXPECT_EQ(buf4, arr3);
    ContentRangeDeque &unloadPages4 = boost::get<2>(res4);
    EXPECT_TRUE(unloadPages4.empty());
  }

  void TestReadDiskFile() {
    string filename = "file2";
    File file1(filename, mtime_);  // empty file
    file1.SetUseDiskFile(true);

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.Write(off1, len1, page1, mtime_);

    const char *page2 = "abc";
    size_t len2 = 3;
    off_t off2 = off_t(len1);
    file1.Write(off2, len2, page2, mtime_);

    const char *page3 = "ABC";
    size_t len3 = 3;
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    file1.Write(off3, len3, page3, mtime_);

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res1 =
        file1.Read(off1, len1, 0);
    EXPECT_EQ(boost::get<0>(res1), len1);
    list<shared_ptr<Page> > &pages1 = boost::get<1>(res1);
    EXPECT_EQ(pages1.size(), 1u);
    array<char, 3> buf1;
    pages1.front()->Read(&buf1[0]);
    array<char, 3> arr1;
    arr1[0] = '0';
    arr1[1] = '1';
    arr1[2] = '2';
    EXPECT_EQ(buf1, arr1);

    tuple<size_t, list<shared_ptr<Page> >, ContentRangeDeque> res2 =
        file1.Read(off1 + 1, len1, 0);
    EXPECT_EQ(boost::get<0>(res2), len1 + len2);
    list<shared_ptr<Page> > &pages2 = boost::get<1>(res2);
    EXPECT_EQ(pages2.size(), 2u);
    array<char, 3> buf2;
    pages2.front()->Read(&buf2[0]);
    EXPECT_EQ(buf2, arr1);
    array<char, 3> buf3;
    pages2.back()->Read(&buf3[0]);
    array<char, 3> arr2;
    arr2[0] = 'a';
    arr2[1] = 'b';
    arr2[2] = 'c';
    EXPECT_EQ(buf3, arr2);
  }
};

TEST_F(FileTest, Default) {
  string filename = "file1";
  string filepath =
      QS::Configure::Options::Instance().GetDiskCacheDirectory() + filename;
  File file1(filename, mtime_);
  EXPECT_EQ(file1.GetBaseName(), filename);
  EXPECT_EQ(file1.GetSize(), 0u);
  EXPECT_EQ(file1.GetCachedSize(), 0u);
  EXPECT_EQ(file1.GetTime(), mtime_);
  EXPECT_FALSE(file1.UseDiskFile());
  EXPECT_EQ(file1.AskDiskFilePath(), filepath);
  EXPECT_TRUE(file1.HasData(0, 0));
  EXPECT_FALSE(file1.HasData(0, 1));
  EXPECT_TRUE(file1.GetUnloadedRanges(0, 0).empty());
  EXPECT_TRUE(file1.GetUnloadedRanges(0, 1).empty());
  pair<PageSetConstIterator, PageSetConstIterator> consecutivePagesAtFront =
      file1.ConsecutivePageRangeAtFront();
  EXPECT_TRUE(consecutivePagesAtFront.first == file1.BeginPage());
  EXPECT_TRUE(consecutivePagesAtFront.second == file1.EndPage());
  EXPECT_TRUE(file1.EndPage() == file1.BeginPage());
  EXPECT_EQ(file1.GetNumPages(), 0u);
}

TEST_F(FileTest, Write) { TestWrite(); }

TEST_F(FileTest, WriteDiskFile) { TestWriteDiskFile(); }

TEST_F(FileTest, Read) { TestRead(); }

TEST_F(FileTest, ReadDiskFile) { TestReadDiskFile(); }

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
