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
#include <time.h>

#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/Entry.h"
#include "data/Node.h"

namespace QS {

namespace Data {

using boost::make_shared;
using boost::scoped_ptr;
using boost::shared_ptr;
using QS::Data::Entry;
using QS::Data::FileType;
using QS::Data::Node;
using std::string;
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

class NodeTest : public Test {
 protected:
  static void SetUpTestCase() {
    InitLog();
    pRootEntry = new Entry(
        "/", 0, mtime_, mtime_, uid_, gid_, fileMode_, FileType::Directory);
    pRootNode = make_shared<Node>(Entry(*pRootEntry));
    pFileNode1 = make_shared<Node>(
         Entry("file1", 1024, mtime_, mtime_, uid_, gid_,
                                    fileMode_, FileType::File),
        pRootNode);
    pLinkNode = make_shared<Node>(
         Entry("linkToFile1", strlen(path), mtime_, mtime_,
                                    uid_, gid_, fileMode_, FileType::SymLink),
        pRootNode, path);
    pEmptyNode.reset(new Node);
  }

  static void TearDownTestCase() {
    delete pRootEntry;
    pRootEntry = NULL;
    pRootNode.reset();
    pFileNode1.reset();
    pLinkNode.reset();
    pEmptyNode.reset();
  }

 protected:
  static const char path[];
  static Entry *pRootEntry;
  static shared_ptr<Node> pRootNode;
  static shared_ptr<Node> pFileNode1;
  static shared_ptr<Node> pLinkNode;
  static scoped_ptr<Node> pEmptyNode;
};

const char NodeTest::path[] = "pathLinkToFile1";
Entry *NodeTest::pRootEntry = NULL;
shared_ptr<Node> NodeTest::pRootNode;
shared_ptr<Node> NodeTest::pFileNode1;
shared_ptr<Node> NodeTest::pLinkNode;
scoped_ptr<Node> NodeTest::pEmptyNode;

TEST_F(NodeTest, DefaultCtor) {
  EXPECT_FALSE(pEmptyNode->operator bool());
  EXPECT_TRUE(pEmptyNode->IsEmpty());
  EXPECT_FALSE(const_cast<const Node*> (pEmptyNode.get())->GetEntry());
  EXPECT_TRUE(pEmptyNode->GetFilePath().empty());
}

TEST_F(NodeTest, CustomCtors) {
  EXPECT_TRUE(pRootNode->operator bool());
  EXPECT_TRUE(pRootNode->IsEmpty());
  EXPECT_EQ(pRootNode->GetFilePath(), "/");
  EXPECT_EQ((const_cast<const Node*>(pRootNode.get())->GetEntry()),
            *pRootEntry);
  EXPECT_EQ(pRootNode->GetFilePath(), pRootEntry->GetFilePath());

  EXPECT_EQ(*(pFileNode1->GetParent()), *pRootNode);

  EXPECT_EQ(pLinkNode->GetSymbolicLink(), string(path));
}

TEST_F(NodeTest, PublicFunctions) {
  // When sharing resources between tests in test case of NodeTest,
  // as the test order is undefined, so we must restore the state
  // to its original value before passing control to the next test.
  EXPECT_FALSE(pRootNode->Find(pFileNode1->GetFilePath()));
  pRootNode->Insert(pFileNode1);
  EXPECT_EQ(pRootNode->Find(pFileNode1->GetFilePath()), pFileNode1);
  EXPECT_EQ(const_cast<const Node*>(pRootNode.get())->GetChildren().size(), 1U);

  EXPECT_FALSE(pRootNode->Find(pLinkNode->GetFilePath()));
  pRootNode->Insert(pLinkNode);
  EXPECT_EQ(pRootNode->Find(pLinkNode->GetFilePath()), pLinkNode);
  EXPECT_EQ(const_cast<const Node*>(pRootNode.get())->GetChildren().size(), 2U);

  string oldFilePath = pFileNode1->GetFilePath();
  string newFilePath("myNewFile1");
  pRootNode->RenameChild(oldFilePath, newFilePath);
  EXPECT_FALSE(pRootNode->Find(oldFilePath));
  EXPECT_TRUE(pRootNode->Find(newFilePath));
  EXPECT_EQ(pFileNode1->GetFilePath(), newFilePath);
  pRootNode->RenameChild(newFilePath, oldFilePath);

  pRootNode->Remove(pFileNode1);
  EXPECT_FALSE(pRootNode->Find(pFileNode1->GetFilePath()));
  pRootNode->Remove(pLinkNode);
  EXPECT_FALSE(pRootNode->Find(pLinkNode->GetFilePath()));
  EXPECT_TRUE(pRootNode->IsEmpty());
}

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
