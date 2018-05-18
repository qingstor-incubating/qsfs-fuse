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
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/File.h"
#include "data/Page.h"

namespace QS {

namespace Data {

using boost::array;
using boost::make_shared;
using boost::shared_ptr;
using boost::tuple;
using QS::Utils::AppendPathDelim;
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

uid_t uid_ = 1000U;
gid_t gid_ = 1000U;
mode_t fileMode_ = S_IRWXU | S_IRWXG | S_IROTH;
shared_ptr<DirectoryTree> nullDirTree = shared_ptr<DirectoryTree>();

class FileTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  void TestWrite(bool useDisk) {
    string filename = "File_TestWrite";
    string filepath =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename;
    File file1(filepath);  // empty file
    if (useDisk) {
      file1.SetUseDiskFile(true);
    }

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.DoWrite(off1, len1, page1);
    EXPECT_EQ(file1.GetSize(), len1);
    EXPECT_EQ(file1.GetDataSize(), len1);
    if (useDisk) {
      EXPECT_EQ(file1.GetCachedSize(), 0u);
    } else {
      EXPECT_EQ(file1.GetCachedSize(), len1);
    }
    EXPECT_EQ(file1.UseDiskFile(), useDisk);
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
    file1.DoWrite(off2, len2, page2);
    EXPECT_EQ(file1.GetSize(), len1 + len2);
    EXPECT_EQ(file1.GetDataSize(), len1 + len2);
    if (useDisk) {
      EXPECT_EQ(file1.GetCachedSize(), 0u);
    } else {
      EXPECT_EQ(file1.GetCachedSize(), len1 + len2);
    }
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
    file1.DoWrite(off3, len3, page3);
    EXPECT_EQ(file1.GetDataSize(), len1 + len2 + len3);
    EXPECT_EQ(file1.GetSize(), len1 + len2 + holeLen + len3);
    if (useDisk) {
      EXPECT_EQ(file1.GetCachedSize(), 0u);
    } else {
      EXPECT_EQ(file1.GetCachedSize(), len1 + len2 + len3);
    }
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

  void TestRead(bool useDisk) {
    string filename = "File_TestRead";
    string filepath =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename;

    File file1(filepath);  // empty file
    if (useDisk) {
      file1.SetUseDiskFile(true);
    }

    const char *page1 = "012";
    size_t len1 = 3;
    off_t off1 = 0;
    file1.DoWrite(off1, len1, page1);

    const char *page2 = "abc";
    size_t len2 = 3;
    off_t off2 = off_t(len1);
    file1.DoWrite(off2, len2, page2);

    const char *page3 = "ABC";
    size_t len3 = 3;
    size_t holeLen = 10;
    off_t off3 = off2 + len2 + holeLen;
    file1.DoWrite(off3, len3, page3);
    // now file1 data should be
    // column: 01234567890123456789
    //
    // data:   012abc          ABC
    //
    {
      array<char, 3> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off1, len1, &buf[0]);
      EXPECT_EQ(res.first, len1);
      array<char, 3> arr;
      arr[0] = '0';
      arr[1] = '1';
      arr[2] = '2';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_TRUE(unloadPages.empty());
    }
    {
      array<char, 3> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off2, len2, &buf[0]);
      EXPECT_EQ(res.first, 3);
      array<char, 3> arr;
      arr[0] = 'a';
      arr[1] = 'b';
      arr[2] = 'c';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_TRUE(unloadPages.empty());
    }
    {
      array<char, 3> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off1 + 1, 3, &buf[0]);
      EXPECT_EQ(res.first, 3);
      array<char, 3> arr;
      arr[0] = '1';
      arr[1] = '2';
      arr[2] = 'a';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_TRUE(unloadPages.empty());

      pair<size_t, ContentRangeDeque> res_ =
          file1.ReadNoLoad(off2 + len2, holeLen, NULL);
      EXPECT_EQ(res_.first, 0u);
      ContentRangeDeque &unloadPages_ = res_.second;
      EXPECT_FALSE(unloadPages_.empty());
    }
    {
      array<char, 3> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off3, len3, &buf[0]);
      EXPECT_EQ(res.first, len3);
      array<char, 3> arr;
      arr[0] = 'A';
      arr[1] = 'B';
      arr[2] = 'C';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_TRUE(unloadPages.empty());
    }
    {
      array<char, 3> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off2, len2 + 1, &buf[0]);
      EXPECT_EQ(res.first, len2);
      array<char, 3> arr;
      arr[0] = 'a';
      arr[1] = 'b';
      arr[2] = 'c';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_EQ(unloadPages.size(), 1u);
      EXPECT_EQ(unloadPages.front(), make_pair(off_t(off2 + len2), 1ul));
    }
    {
      array<char, 13> buf;
      pair<size_t, ContentRangeDeque> res =
          file1.ReadNoLoad(off2 + len2, holeLen + len3, &buf[0]);
      EXPECT_EQ(res.first, len3);
      array<char, 13> arr;
      for (int i = 0; i < 10; ++i) {
        arr[i] = '\0';
      }
      arr[10] = 'A';
      arr[11] = 'B';
      arr[12] = 'C';
      EXPECT_EQ(buf, arr);
      ContentRangeDeque &unloadPages = res.second;
      EXPECT_EQ(unloadPages.size(), 1u);
      EXPECT_EQ(unloadPages.front(),
                make_pair(off_t(off2 + len2), size_t(holeLen)));
    }

    {
        array<char, 13> buf;
        pair<size_t, ContentRangeDeque> res =
            file1.ReadNoLoad(off2 + len2, holeLen + len3 + 1, &buf[0]);
        EXPECT_EQ(res.first, len3);
        array<char, 13> arr;
        for (int i = 0; i < 10; ++i) {
          arr[i] = '\0';
        }
        arr[10] = 'A';
        arr[11] = 'B';
        arr[12] = 'C';
        EXPECT_EQ(buf, arr);
        ContentRangeDeque &unloadPages = res.second;
        EXPECT_EQ(unloadPages.size(), 2u);
        EXPECT_EQ(unloadPages.front(),
                  make_pair(off_t(off2 + len2), size_t(holeLen)));
        EXPECT_EQ(unloadPages.back(),
                  make_pair(off_t(off3 + len3), size_t(1u)));
      }
  }

  void TestResize(bool useDisk) {
    uint64_t cacheCap = 100;
    if (useDisk) {
      cacheCap = 3;
    }
    shared_ptr<Cache> cache = shared_ptr<Cache>(new Cache(cacheCap));

    const char *filename = "File_TestResize";
    const char *page1 = "012";
    size_t len1 = strlen(page1);
    off_t off1 = 0;
    shared_ptr<File> file = cache->MakeFile(filename);
    file->Write(off1, len1, page1, nullDirTree, cache);
    const char *data = "abc";
    size_t len2 = strlen(data);
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    file->Write(off2, len2, page2, nullDirTree, cache);
    const char *page3 = "ABC";
    size_t len3 = strlen(page3);
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    file->Write(off3, len3, page3, nullDirTree,cache);

    EXPECT_EQ(file->GetSize(), len1 + len2 + holeLen + len3);
    EXPECT_EQ(file->GetDataSize(), len1 + len2 + len3);

    size_t newFileSz = len1 + len2 + holeLen + len3 + 1;
    file->Resize(newFileSz, nullDirTree, cache);
    EXPECT_EQ(file->GetDataSize(), len1 + len2 + len3 + 1);
    EXPECT_EQ(file->GetSize(), newFileSz);

    size_t newFileSz1 = len1 + len2 + 1;
    file->Resize(newFileSz1, nullDirTree, cache);
    EXPECT_EQ(file->GetDataSize(), len1 + len2);
    EXPECT_EQ(file->GetSize(), newFileSz1);

    size_t newFileSz2 = len1 + 1;
    file->Resize(newFileSz2, nullDirTree, cache);
    EXPECT_EQ(file->GetDataSize(), newFileSz2);
    EXPECT_EQ(file->GetSize(), newFileSz2);
  }
};

TEST_F(FileTest, Default) {
  string filename = "File_TestDefault";
  string filepath =
      AppendPathDelim(
          QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
      filename;
  File file1(filepath);
  EXPECT_EQ(file1.GetBaseName(), filename);
  EXPECT_EQ(file1.GetSize(), 0u);
  EXPECT_EQ(file1.GetCachedSize(), 0u);
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

TEST_F(FileTest, Write) { TestWrite(false); }

TEST_F(FileTest, WriteDiskFile) { TestWrite(true); }

TEST_F(FileTest, Read) { TestRead(false); }

TEST_F(FileTest, ReadDiskFile) { TestRead(true); }

TEST_F(FileTest, Resize) { TestResize(false); }

TEST_F(FileTest, ResizeDiskFile) { TestResize(true); }

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
