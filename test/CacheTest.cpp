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

#include <sstream>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"

namespace QS {

namespace Data {

using boost::make_shared;
using boost::shared_ptr;
using std::make_pair;
using std::stringstream;
using std::vector;
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
shared_ptr<DirectoryTree> dirTree = shared_ptr<DirectoryTree>();

class CacheTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  // --------------------------------------------------------------------------
  void TestDefault() {
    uint64_t cacheCap = 100;
    Cache cache(cacheCap);
    EXPECT_TRUE(cache.HasFreeSpace(cacheCap));
    EXPECT_FALSE(cache.HasFreeSpace(cacheCap + 1));
    EXPECT_EQ(cache.GetSize(), 0u);
    EXPECT_EQ(cache.GetCapacity(), cacheCap);
    EXPECT_EQ(cache.GetNumFile(), 0u);
    EXPECT_TRUE(cache.Begin() == cache.End());
    EXPECT_TRUE(cache.Free(cacheCap, ""));
    EXPECT_FALSE(cache.IsLastFileOpen());
  }

  // --------------------------------------------------------------------------
  void TestWrite() {
    uint64_t cacheCap = 100;
    Cache cache(cacheCap);

    const char *page1 = "012";
    size_t len1 = strlen(page1);
    off_t off1 = 0;
    cache.Write("file1", off1, len1, page1, dirTree);
    EXPECT_FALSE(cache.HasFreeSpace(cacheCap));
    EXPECT_TRUE(cache.HasFreeSpace(cacheCap - len1));
    EXPECT_EQ(cache.GetSize(), len1);
    EXPECT_EQ(cache.GetCapacity(), cacheCap);
    EXPECT_EQ(cache.GetNumFile(), 1u);
    EXPECT_TRUE(cache.Begin() != cache.End());
    EXPECT_TRUE(cache.FindFile("file1"));
    EXPECT_TRUE(cache.HasFile("file1"));
    EXPECT_TRUE(cache.HasFileData("file1", off1, len1));
    EXPECT_FALSE(cache.HasFileData("file1", off1, len1 + 1));
    EXPECT_FALSE(cache.HasFileData("file1", off1 + 1, len1));
    EXPECT_TRUE(cache.GetUnloadedRanges("file1", 0, len1).empty());
    EXPECT_FALSE(cache.GetUnloadedRanges("file1", 0, len1 + 1).empty());
    ContentRangeDeque range1;
    range1.push_back(make_pair(len1, 1));
    EXPECT_EQ(cache.GetUnloadedRanges("file1", 0, len1 + 1), range1);

    EXPECT_FALSE(cache.IsLastFileOpen());
    cache.SetFileOpen("file1", true, dirTree);
    EXPECT_TRUE(cache.IsLastFileOpen());
    cache.SetFileOpen("file1", false, dirTree);

    size_t newSize = 2;
    cache.Resize("file1", newSize, dirTree);
    EXPECT_EQ(cache.GetSize(), newSize);

    cache.Rename("file1", "newfile1");
    EXPECT_FALSE(cache.HasFile("file1"));
    EXPECT_TRUE(cache.HasFile("newfile1"));

    EXPECT_FALSE(cache.Free(cacheCap, "newfile1"));
    EXPECT_TRUE(cache.Free(cacheCap, ""));
    EXPECT_FALSE(cache.HasFile("newfile1"));

    EXPECT_TRUE(cache.HasFreeSpace(cacheCap));
    cache.Write("file1", off1, len1, page1,
                dirTree);  // write again after free
    EXPECT_TRUE(cache.HasFile("file1"));
    EXPECT_EQ(cache.Erase("file1"), cache.End());
    EXPECT_FALSE(cache.HasFile("file1"));
    EXPECT_TRUE(cache.HasFreeSpace(cacheCap));
    cache.Write("file1", off1, len1, page1,
                dirTree);  // write again after erase
    EXPECT_TRUE(cache.HasFile("file1"));

    const char *data = "abc";
    size_t len2 = strlen(data);
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    cache.Write("file1", off2, len2, page2, dirTree);
    EXPECT_EQ(cache.GetNumFile(), 1u);
    EXPECT_EQ(cache.GetSize(), len1 + len2);
    EXPECT_TRUE(cache.GetUnloadedRanges("file1", 0, len1 + len2).empty());
    ContentRangeDeque range2;
    range2.push_back(make_pair(len1 + len2, 1));
    EXPECT_EQ(cache.GetUnloadedRanges("file1", 0, len1 + len2 + 1), range2);

    const char *page3 = "ABC";
    size_t len3 = strlen(page3);
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    cache.Write("file1", off3, len3, page3, dirTree);
    EXPECT_EQ(cache.GetSize(), len1 + len2 + len3);
    EXPECT_TRUE(cache.HasFileData("file1", off1, len1 + len2));
    EXPECT_FALSE(cache.HasFileData("file1", off1, len1 + len2 + 1));
    EXPECT_FALSE(cache.HasFileData("file1", off2 + len2, 1));
    ContentRangeDeque range3;
    range3.push_back(make_pair(len1 + len2, holeLen));
    EXPECT_EQ(cache.GetUnloadedRanges("file1", 0, len1 + len2 + holeLen + len3),
              range3);

    cache.Write("file2", off1, len1, page1, dirTree);
    EXPECT_EQ(cache.GetNumFile(), 2u);
    EXPECT_EQ(cache.GetSize(), 2 * len1 + len2 + len3);
    EXPECT_TRUE(cache.FindFile("file2"));
    EXPECT_TRUE(cache.HasFile("file1"));
    EXPECT_TRUE(cache.FindFile("file1"));
    EXPECT_TRUE(cache.Free(cacheCap - len1, "file2"));
    EXPECT_FALSE(cache.HasFile("file1"));
    EXPECT_EQ(cache.GetSize(), len1);
  }

  // --------------------------------------------------------------------------
  void TestWriteDiskFile() {
    uint64_t cacheCap = 3;
    Cache cache(cacheCap);

    const char *page1 = "012";
    size_t len1 = strlen(page1);
    off_t off1 = 0;
    cache.Write("file1", off1, len1, page1, dirTree);

    shared_ptr<File> pfile = cache.FindFile("file1");
    EXPECT_TRUE(pfile);
    //EXPECT_EQ(pfile, cache.Begin());

    const char *data = "abc";
    size_t len2 = strlen(data);
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    cache.Write("file1", off2, len2, page2, dirTree);
    const char *page3 = "ABC";
    size_t len3 = strlen(page3);
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    cache.Write("file1", off3, len3, page3, dirTree);

    EXPECT_EQ(cache.GetFileSize("file1"), len1 + len2 + len3);
    //pfile = cache.Find("file1");
    EXPECT_TRUE(pfile->UseDiskFile());

    EXPECT_EQ(cache.GetSize(), len1);
    EXPECT_TRUE(cache.HasFileData("file1", off1, len1 + len2));
    EXPECT_FALSE(cache.HasFileData("file1", off1, len1 + len2 + 1));
    EXPECT_FALSE(cache.HasFileData("file1", off2 + len2, 1));
    ContentRangeDeque range3;
    range3.push_back(make_pair(len1 + len2, holeLen));
    EXPECT_EQ(cache.GetUnloadedRanges("file1", 0, len1 + len2 + holeLen + len3),
              range3);

    cache.Write("file2", off1, len1, page1, dirTree);
    EXPECT_FALSE(cache.HasFile("file1"));
    EXPECT_EQ(cache.GetNumFile(), 1u);
    EXPECT_EQ(cache.GetSize(), len1);
    //EXPECT_EQ(cache.Find("file2"), cache.Begin());
    EXPECT_TRUE(cache.FindFile("file2"));
    EXPECT_TRUE(cache.Free(cacheCap, ""));
    EXPECT_FALSE(cache.HasFile("file2"));
    EXPECT_EQ(cache.GetSize(), 0u);
  }

  // --------------------------------------------------------------------------
  void TestResize() {
    uint64_t cacheCap = 100;
    Cache cache(cacheCap);

    const char *page1 = "012";
    size_t len1 = strlen(page1);
    off_t off1 = 0;
    cache.Write("file1", off1, len1, page1, dirTree);
    const char *data = "abc";
    size_t len2 = strlen(data);
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    cache.Write("file1", off2, len2, page2, dirTree);
    const char *page3 = "ABC";
    size_t len3 = strlen(page3);
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    cache.Write("file1", off3, len3, page3, dirTree);
    cache.Write("file2", off1, len1, page1, dirTree);

    EXPECT_EQ(cache.GetFileSize("file1"), len1 + len2 + len3);
    EXPECT_EQ(cache.GetFileSize("file2"), len1);

    size_t newFile1Sz = len1 + len2 + 1;
    cache.Resize("file1", newFile1Sz, dirTree);
    EXPECT_EQ(cache.GetFileSize("file1"), newFile1Sz);
    size_t newFile2Sz = len1 - 1;
    cache.Resize("file2", newFile2Sz, dirTree);
    EXPECT_EQ(cache.GetFileSize("file2"), newFile2Sz);
  }

  // --------------------------------------------------------------------------
  void TestResizeDiskFile() {
    uint64_t cacheCap = 3;
    Cache cache(cacheCap);

    const char *page1 = "012";
    size_t len1 = strlen(page1);
    off_t off1 = 0;
    cache.Write("file1", off1, len1, page1, dirTree);
    const char *data = "abc";
    size_t len2 = strlen(data);
    off_t off2 = off_t(len1);
    shared_ptr<stringstream> page2 = make_shared<stringstream>(data);
    cache.Write("file1", off2, len2, page2, dirTree);
    const char *page3 = "ABC";
    size_t len3 = strlen(page3);
    size_t holeLen = 10;
    off_t off3 = off2 + holeLen + len3;
    cache.Write("file1", off3, len3, page3, dirTree);
    EXPECT_EQ(cache.GetFileSize("file1"), len1 + len2 + len3);
    size_t newFile1Sz = len1 + len2 + 1;
    cache.Resize("file1", newFile1Sz, dirTree);
    EXPECT_EQ(cache.GetFileSize("file1"), newFile1Sz);

    cache.Write("file2", off1, len1, page1, dirTree);
    EXPECT_FALSE(cache.HasFile("file1"));
    EXPECT_EQ(cache.GetFileSize("file2"), len1);
    size_t newFile2Sz = len1 - 1;
    cache.Resize("file2", newFile2Sz, dirTree);
    EXPECT_EQ(cache.GetFileSize("file2"), newFile2Sz);
  }

  

};

TEST_F(CacheTest, Default) { TestDefault(); }

TEST_F(CacheTest, Write) { TestWrite(); }

TEST_F(CacheTest, WriteDiskFile) { TestWriteDiskFile(); }

TEST_F(CacheTest, Resize) { TestResize(); }

TEST_F(CacheTest, ResizeDiskFile) { TestResizeDiskFile(); }


}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
