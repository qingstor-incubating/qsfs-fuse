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

#include <ostream>
#include <string>
#include <utility>

#include "gtest/gtest.h"

#include "boost/shared_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/Entry.h"
#include "data/FileMetaData.h"

namespace QS {

namespace Data {

using boost::shared_ptr;
using QS::Data::Entry;
using QS::Data::FileMetaData;
using QS::Data::FileType;

using std::ostream;
using std::string;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;

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

struct MetaData {
  string filePath;
  uint64_t fileSize;
  FileType::Value fileType;
  int numLink;
  bool isDir;
  bool isOperable;

  MetaData(const string &path, uint64_t size, FileType::Value type, int nLink,
           bool dir, bool op)
      : filePath(path),
        fileSize(size),
        fileType(type),
        numLink(nLink),
        isDir(dir),
        isOperable(op) {}

  friend ostream &operator<<(ostream &os, const MetaData &meta) {
    return os << "FileName: " << meta.filePath << " FileSize: " << meta.fileSize
              << " FileType: " << QS::Data::GetFileTypeName(meta.fileType)
              << " NumLink: " << meta.numLink << " IsDir: " << meta.isDir
              << " IsOperable: " << meta.isOperable;
  }
};

class EntryTest : public Test, public WithParamInterface<MetaData> {
 public:
  EntryTest() {
    InitLog();
    MetaData meta = GetParam();
    m_pFileMetaData.reset(new FileMetaData(meta.filePath, meta.fileSize, mtime_,
                                           mtime_, uid_, gid_, fileMode_,
                                           meta.fileType));
    m_pEntry = new Entry(meta.filePath, meta.fileSize, mtime_, mtime_, uid_,
                         gid_, fileMode_, meta.fileType);
  }

  void TestCopyControl() {
    MetaData meta = GetParam();
    EXPECT_EQ(m_pEntry->operator bool(), meta.isOperable);
    Entry entry = Entry(m_pFileMetaData);  // Manager will replace meta data
    EXPECT_TRUE(entry);
    // when construct entry with same meta data, manager will update it
    // by replace the shared_ptr, at this moment the previous share_ptr
    // will decrease it ref count to 0, shared_ptr destory referenced object,
    // So weak_ptr hold by Entry get expired.
    EXPECT_FALSE(*m_pEntry);  // weak_ptr get expired
  }

  ~EntryTest() {
    delete m_pEntry;
    m_pEntry = NULL;
  }

 protected:
  shared_ptr<FileMetaData> m_pFileMetaData;
  Entry *m_pEntry;
};

TEST_P(EntryTest, CopyControl) { TestCopyControl(); }

TEST_P(EntryTest, PublicFunctions) {
  MetaData meta = GetParam();
  EXPECT_EQ(m_pEntry->GetFilePath(), meta.filePath);
  EXPECT_EQ(m_pEntry->GetFileSize(), meta.fileSize);
  EXPECT_EQ(m_pEntry->GetFileType(), meta.fileType);
  EXPECT_EQ(m_pEntry->GetNumLink(), meta.numLink);
  EXPECT_EQ(m_pEntry->IsDirectory(), meta.isDir);
  EXPECT_EQ(m_pEntry->operator bool(), meta.isOperable);
}

INSTANTIATE_TEST_CASE_P(
    FSEntryTest, EntryTest,
    // filePath, fileSize, fileType, numLink, isDir, isOperable
    Values(MetaData("/", 0, FileType::Directory, 2, true, true),
           MetaData("/file1", 0, FileType::File, 1, false, true),
           MetaData("/file2", 1024, FileType::File, 1, false, true)));

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
