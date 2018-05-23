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
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "configure/Options.h"
#include "data/Cache.h"
#include "data/DirectoryTree.h"
#include "data/File.h"

namespace QS {

namespace Data {

using boost::make_shared;
using boost::shared_ptr;
using QS::Utils::AppendPathDelim;
using std::make_pair;
using std::string;
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
    cache.Free(10, "");
    EXPECT_EQ(cache.GetSize(), 0u);
    EXPECT_EQ(cache.GetCapacity(), cacheCap);
    EXPECT_TRUE(cache.Free(cacheCap, ""));
  }

  // --------------------------------------------------------------------------
  void TestNewFile(bool useDisk) {
    uint64_t cacheCap = useDisk ? 0 : 100;
    Cache cache(cacheCap);

    const char *filename = "file1";
    string filepath =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename;
    cache.MakeFile(filepath);
    EXPECT_EQ(cache.GetNumFile(), 1u);
    shared_ptr<File> file = cache.FindFile(filepath);
    EXPECT_TRUE(file);
    EXPECT_TRUE(cache.HasFile(filepath));
  }

  // --------------------------------------------------------------------------
  void TestEraseFile(bool useDisk) {
    uint64_t cacheCap = useDisk ? 0 : 100;
    Cache cache(cacheCap);

    const char *filename = "file1";
    string filepath =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename;
    cache.MakeFile(filepath);
    EXPECT_TRUE(cache.HasFile(filepath));
    cache.Erase(filepath);
    EXPECT_FALSE(cache.HasFile(filepath));
  }

  // --------------------------------------------------------------------------
  void TestRenameFile(bool useDisk) {
    uint64_t cacheCap = useDisk ? 0 : 100;
    Cache cache(cacheCap);

    const char *filename = "file1";
    const char *filenameN = "file1_rename";
    string filepath =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename;
    string filepathN =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filenameN;
    cache.MakeFile(filepath);
    EXPECT_TRUE(cache.HasFile(filepath));
    cache.Rename(filepath, filepathN);
    EXPECT_FALSE(cache.HasFile(filepath));
    EXPECT_TRUE(cache.HasFile(filepathN));
  }

  // --------------------------------------------------------------------------
  void TestMakeFileMostRecently() {
    uint64_t cacheCap = 100;
    Cache cache(cacheCap);

    const char *filename1 = "file1";
    const char *filename2 = "file2";
    string filepath1 =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename1;
    string filepath2 =
        AppendPathDelim(
            QS::Configure::Options::Instance().GetDiskCacheDirectory()) +
        filename2;
    cache.MakeFile(filepath1);
    cache.MakeFile(filepath2);
    EXPECT_EQ(cache.Begin()->first, filepath2);
    cache.MakeFileMostRecentlyUsed(filepath1);
    EXPECT_EQ(cache.Begin()->first, filepath1);
  }
};

TEST_F(CacheTest, Default) { TestDefault(); }

TEST_F(CacheTest, NewFile) { TestNewFile(false); }

TEST_F(CacheTest, EraseFile) { TestEraseFile(false); }

TEST_F(CacheTest, RenameFile) { TestRenameFile(false); }

TEST_F(CacheTest, MakeFileMostRecently) { TestMakeFileMostRecently(); }

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
