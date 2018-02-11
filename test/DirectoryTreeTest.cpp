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

#include <time.h>

#include <unistd.h>

#include <vector>

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/weak_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/DirectoryTree.h"
#include "data/FileMetaData.h"
#include "data/Node.h"

namespace QS {

namespace Data {

using boost::make_shared;
using boost::shared_ptr;
using boost::weak_ptr;
using std::vector;
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
mode_t dirMode_ = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
mode_t fileMode_ = S_IRWXU | S_IRWXG | S_IROTH;
mode_t rootMode_ = S_IRWXU | S_IRWXG | S_IRWXO;

class DirectoryTreeTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  void OperationsTest1() {
    DirectoryTree tree(mtime_, uid_, gid_, rootMode_);
    shared_ptr<FileMetaData> file1 = make_shared<FileMetaData>(
        "/file1", 10, mtime_, mtime_, uid_, gid_, fileMode_, FileType::File);
    tree.Grow(file1);
    EXPECT_TRUE(tree.Has("/file1"));
    shared_ptr<Node> node_file1 = tree.Find("/file1");
    EXPECT_EQ(*(const_cast<const Node *>(node_file1.get())
                    ->GetEntry()
                    .GetMetaData()
                    .lock()),
              *file1);

    shared_ptr<FileMetaData> dir1 =
        make_shared<FileMetaData>("/folder1", 1024, mtime_, mtime_, uid_, gid_,
                                  dirMode_, FileType::Directory);
    tree.Grow(dir1);
    // Notice: directory will be appended with "/" automatically
    // When you try to find it, remember to append "/" at end
    EXPECT_TRUE(tree.Has("/folder1/"));
    {
      vector<weak_ptr<Node> > childs = tree.FindChildren("/");
      EXPECT_EQ(childs.size(), 2U);
    }

    shared_ptr<FileMetaData> file1InFolder =
        make_shared<FileMetaData>("/folder1/file1", 10, mtime_, mtime_, uid_,
                                  gid_, fileMode_, FileType::File);
    tree.Grow(file1InFolder);
    EXPECT_EQ(tree.FindChildren("/").size(), 2U);
    EXPECT_EQ(tree.FindChildren("/folder1/").size(), 1U);
    shared_ptr<Node> node_dir1 = tree.Find("/folder1/");
    EXPECT_TRUE(node_dir1->HaveChild("/folder1/file1"));
    EXPECT_EQ(*(node_dir1->GetChildrenIds().begin()),
              file1InFolder->GetFilePath());
    shared_ptr<Node> node_file1InFolder = tree.Find("/folder1/file1");
    EXPECT_EQ(node_file1InFolder->GetParent()->GetFilePath(),
              dir1->GetFilePath());

    // Notice: the dir name is appended "/" automatically
    shared_ptr<Node> node_dir2 = tree.Rename("/folder1/", "/folder2/");
    EXPECT_FALSE(tree.Has("/folder1/"));
    EXPECT_TRUE(tree.Has("/folder2/"));
    EXPECT_TRUE(node_dir2->HaveChild("/folder2/file1"));
    EXPECT_EQ(node_file1InFolder->GetParent()->GetFilePath(),
              node_dir2->GetFilePath());

    tree.Remove("/file1");
    EXPECT_FALSE(tree.Has("/file1"));
    EXPECT_EQ(tree.FindChildren("/").size(), 1U);

    tree.Remove("/folder2/");
    EXPECT_FALSE(tree.Has("/folder2/"));
    EXPECT_EQ(tree.FindChildren("/").size(), 0U);
  }

  void OperationsTest2() {
    DirectoryTree tree(mtime_, uid_, gid_, rootMode_);
    shared_ptr<FileMetaData> dir1 =
        make_shared<FileMetaData>("/folder1", 1024, mtime_, mtime_, uid_, gid_,
                                  dirMode_, FileType::Directory);
    tree.Grow(dir1);
    shared_ptr<FileMetaData> file1InFolder =
        make_shared<FileMetaData>("/folder1/file1", 10, mtime_, mtime_, uid_,
                                  gid_, fileMode_, FileType::File);
    tree.Grow(file1InFolder);

    shared_ptr<FileMetaData> file1InFolder_ =
        make_shared<FileMetaData>("/folder1/file1", 100, mtime_, mtime_, uid_,
                                  gid_, fileMode_, FileType::File);
    shared_ptr<FileMetaData> file2InFolder =
        make_shared<FileMetaData>("/folder1/file2", 10, mtime_, mtime_, uid_,
                                  gid_, fileMode_, FileType::File);
    shared_ptr<FileMetaData> folder1InFolder =
        make_shared<FileMetaData>("/folder1/folder1", 1024, mtime_, mtime_,
                                  uid_, gid_, dirMode_, FileType::Directory);
    vector<shared_ptr<FileMetaData> > newMetas;
    newMetas.reserve(3);
    newMetas.push_back(file1InFolder_);
    newMetas.push_back(file2InFolder);
    newMetas.push_back(folder1InFolder);
    tree.UpdateDirectory("/folder1/", newMetas);
    EXPECT_TRUE(tree.Has("/folder1/file1"));
    EXPECT_TRUE(tree.Has("/folder1/file2"));
    EXPECT_TRUE(tree.Has("/folder1/folder1/"));
    EXPECT_EQ(tree.FindChildren("/").size(), 1U);
    EXPECT_EQ(tree.FindChildren("/folder1/").size(), 3U);

    shared_ptr<FileMetaData> file1InFolder11 =
        make_shared<FileMetaData>("/folder1/folder1/file1", 100, mtime_, mtime_,
                                  uid_, gid_, fileMode_, FileType::File);
    tree.Grow(file1InFolder11);
    EXPECT_TRUE(tree.Has("/folder1/folder1/file1"));
    EXPECT_EQ(tree.FindChildren("/folder1/folder1/").size(), 1U);

    newMetas.pop_back();
    tree.UpdateDirectory("/folder1/", newMetas);
    EXPECT_TRUE(tree.Has("/folder1/file1"));
    EXPECT_TRUE(tree.Has("/folder1/file2"));
    EXPECT_FALSE(tree.Has("/folder1/folder1/"));
    EXPECT_FALSE(tree.Has("/folder1/folder1/file1"));
    EXPECT_EQ(tree.FindChildren("/").size(), 1U);
    EXPECT_EQ(tree.FindChildren("/folder1/").size(), 2U);
  }
};

TEST_F(DirectoryTreeTest, Ctor) {
  DirectoryTree tree(mtime_, uid_, gid_, rootMode_);
  EXPECT_TRUE(tree.Has("/"));
  EXPECT_TRUE(tree.Find("/"));
  shared_ptr<Node> root = tree.GetRoot();
  EXPECT_TRUE(root);
  EXPECT_EQ(root->GetUID(), uid_);
  EXPECT_EQ(root->GetMTime(), mtime_);
  EXPECT_EQ(root->GetFileMode(), rootMode_);
}

TEST_F(DirectoryTreeTest, Operations1) { OperationsTest1(); }

TEST_F(DirectoryTreeTest, Operations2) { OperationsTest2(); }

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
