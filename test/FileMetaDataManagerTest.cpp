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

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/FileMetaData.h"
#include "data/FileMetaDataManager.h"

namespace QS {

namespace Data {

using boost::make_shared;
using ::testing::Test;

// default log dir
static const char *defaultLogDir = "/tmp/qsfs.test.logs/";
void InitLog() {
  QS::Utils::CreateDirectoryIfNotExists(defaultLogDir);
  QS::Logging::Log::Instance().Initialize(defaultLogDir);
}

// default values for non interested attributes
time_t mtime_ = time(NULL);
uid_t uid_ = 1000U;
gid_t gid_ = 1000U;
mode_t fileMode_ = S_IRWXU | S_IRWXG | S_IROTH;

size_t maxcount = 2;
FileMetaDataManager &manager = FileMetaDataManager::Instance();

class FileMetaDataManagerTest : public Test {
 protected:
  static void SetUpTestCase() {
    InitLog();
    // just for test
    manager.m_maxCount = maxcount;
  }

  void TestDefault() {
    EXPECT_EQ(manager.GetMaxCount(), maxcount);
    EXPECT_TRUE(manager.HasFreeSpace(maxcount));
    EXPECT_TRUE(manager.Get("") == manager.End());
    EXPECT_TRUE(manager.Begin() == manager.m_metaDatas.begin());
    EXPECT_TRUE(manager.End() == manager.m_metaDatas.end());
    EXPECT_TRUE(manager.Begin() == manager.End());
    EXPECT_FALSE(manager.Has(""));
  }

  void TestAddRemove() {
    FileMetaData file1("file1", 2, mtime_, mtime_, uid_, gid_, fileMode_,
                       FileType::File);
    manager.Add(make_shared<FileMetaData>(file1));
    EXPECT_TRUE(manager.HasFreeSpace(maxcount - 1));
    EXPECT_FALSE(manager.HasFreeSpace(maxcount));
    EXPECT_TRUE(manager.Has("file1"));
    EXPECT_TRUE(*(manager.Get("file1")->second) == file1);

    FileMetaData folder1("folder1", 0, mtime_, mtime_, uid_, gid_, fileMode_,
                         FileType::Directory);  // directory will append '/'
    manager.Add(make_shared<FileMetaData>(folder1));
    EXPECT_TRUE(manager.HasFreeSpace(maxcount - 2));
    EXPECT_FALSE(manager.HasFreeSpace(maxcount - 1));
    EXPECT_TRUE(manager.Has("folder1/"));
    EXPECT_TRUE(*(manager.Get("folder1/")->second) == folder1);
    EXPECT_TRUE(*(manager.Begin()->second) == folder1);

    EXPECT_TRUE(manager.Has("file1"));  // will put file1 at front
    EXPECT_TRUE(*(manager.Begin()->second) == file1);

    manager.Erase("file1");
    EXPECT_FALSE(manager.Has("file1"));
    EXPECT_TRUE(manager.HasFreeSpace(maxcount - 1));
    EXPECT_FALSE(manager.HasFreeSpace(maxcount));

    manager.Clear();
    EXPECT_FALSE(manager.Has("folder1/"));
    EXPECT_TRUE(manager.HasFreeSpace(maxcount));
  }

  void TestRename() {
    FileMetaData file1("file1", 2, mtime_, mtime_, uid_, gid_, fileMode_,
                       FileType::File);
    manager.Add(make_shared<FileMetaData>(file1));
    manager.Rename("file1", "newfile1");
    EXPECT_TRUE(manager.Has("newfile1"));
    EXPECT_FALSE(manager.Has("file1"));
    manager.Clear();
  }

  void TestOverflow() {
    FileMetaData file1("file1", 2, mtime_, mtime_, uid_, gid_, fileMode_,
                       FileType::File);
    FileMetaData file2("file2", 2, mtime_, mtime_, uid_, gid_, fileMode_,
                       FileType::File);
    FileMetaData folder1("folder1/", 0, mtime_, mtime_, uid_, gid_, fileMode_,
                         FileType::Directory);

    manager.Add(make_shared<FileMetaData>(file1));
    manager.Add(make_shared<FileMetaData>(folder1));
    EXPECT_TRUE(manager.Has("file1"));
    EXPECT_TRUE(manager.Has("folder1/"));
    EXPECT_TRUE(*(manager.Begin()->second) == folder1);

    manager.Add(make_shared<FileMetaData>(file2));
    EXPECT_TRUE(*(manager.Begin()->second) == file2);
    EXPECT_TRUE(manager.Has("file2"));
    EXPECT_TRUE(manager.Has("folder1/"));
    //EXPECT_FALSE(manager.Has("file1"));

    manager.Clear();
  }
};

TEST_F(FileMetaDataManagerTest, Default) { TestDefault(); }

TEST_F(FileMetaDataManagerTest, AddRemove) { TestAddRemove(); }

TEST_F(FileMetaDataManagerTest, Rename) { TestRename(); }

TEST_F(FileMetaDataManagerTest, Overflow) { TestOverflow(); }

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
