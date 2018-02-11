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

#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "boost/make_shared.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/IOStream.h"
#include "data/StreamBuf.h"
#include "data/StreamUtils.h"

namespace QS {

namespace Data {

using boost::scoped_ptr;
using QS::Data::IOStream;
using QS::Data::StreamBuf;
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

void InitStreamWithNullBuffer() { StreamBuf streamBuf(Buffer(), 1); }

void InitStreamWithOverflowLength() {
  Buffer buf(new vector<char>(1));
  StreamBuf streamBuf(buf, 2);
}

class StreamBufTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  void TestPrivateFun() {
    vector<char> buf;
    buf.reserve(3);
    buf.push_back('0');
    buf.push_back('1');
    buf.push_back('2');
    Buffer buf_(new vector<char>(buf));
    StreamBuf streamBuf(buf_, buf.size() - 1);
    EXPECT_TRUE(*(streamBuf.GetBuffer()) == buf);
    EXPECT_EQ(*(streamBuf.begin()), '0');
    EXPECT_EQ(*(streamBuf.end()), '2');
  }
};

TEST_F(StreamBufTest, DeathTestInitNull) {
  ASSERT_DEATH(InitStreamWithNullBuffer(), "");
}

TEST_F(StreamBufTest, DeathTestInitOverflow) {
  ASSERT_DEATH(InitStreamWithOverflowLength(), "");
}

TEST_F(StreamBufTest, Ctor) {
  vector<char> buf;
  buf.reserve(3);
  buf.push_back('0');
  buf.push_back('1');
  buf.push_back('2');

  Buffer buf_(new vector<char>(buf));
  const StreamBuf streamBuf(buf_, buf.size());
  EXPECT_TRUE(*(streamBuf.GetBuffer()) == buf);
}

TEST_F(StreamBufTest, PrivateFunc) { TestPrivateFun(); }

TEST(IOStreamTest, Ctor1) {
  IOStream iostream(10);
  iostream.seekg(0, std::ios_base::beg);
  StreamBuf * buf = dynamic_cast<StreamBuf *>(iostream.rdbuf());
  EXPECT_EQ(const_cast<const StreamBuf *>(buf)->GetBuffer()->size(), 10u);
  vector<char> buf1 = vector<char>(10);
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(buf)->GetBuffer()) == buf1);
}


TEST(IOStreamTest, Ctor2) {
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back('0');
  buf0.push_back('1');
  buf0.push_back('2');
  Buffer buf(new vector<char>(buf0));

  IOStream iostream(buf, buf->size());
  iostream.seekg(0, std::ios_base::beg);
  StreamBuf *streambuf = dynamic_cast<StreamBuf *>(iostream.rdbuf());
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(streambuf)->GetBuffer()) == buf0);

  Buffer buf1(new vector<char>(buf0));

  IOStream iostream1(buf1, 2);
  iostream1.seekg(0, std::ios_base::beg);
  StreamBuf *streambuf1 = dynamic_cast<StreamBuf *>(iostream1.rdbuf());
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(streambuf1)->GetBuffer()) ==
              buf0);
}

TEST(IOStreamTest, Read1) {
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back('0');
  buf0.push_back('1');
  buf0.push_back('2');
  Buffer buf(new vector<char>(buf0));

  IOStream stream(buf, 3);
  stringstream ss;
  stream.seekg(0, std::ios_base::beg);
  ss << stream.rdbuf();
  EXPECT_EQ(ss.str(), string("012"));

  IOStream stream1(buf, 2);
  stringstream ss1;
  stream1.seekg(0, std::ios_base::beg);
  ss1 << stream1.rdbuf();
  EXPECT_EQ(ss1.str(), string("01"));
}

TEST(IOStreamTest, Read2) {
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back('0');
  buf0.push_back('1');
  buf0.push_back('2');
  Buffer buf(new vector<char>(buf0));

  IOStream stream(buf, 3);
  stream.seekg(1, std::ios_base::beg);
  stringstream ss;
  ss << stream.rdbuf();
  EXPECT_EQ(ss.str(), string("12"));
}

TEST(IOStreamTest, Write1) {
  Buffer buf(new vector<char>(3));
  IOStream stream(buf, 3);
  stringstream ss("012");
  stream << ss.rdbuf();
  stream.seekg(0, std::ios_base::beg);
  StreamBuf *streambuf = dynamic_cast<StreamBuf *>(stream.rdbuf());
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back('0');
  buf0.push_back('1');
  buf0.push_back('2');
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(streambuf)->GetBuffer()) == buf0);

  Buffer buf1(new vector<char>(2));
  IOStream stream1(buf1, 2);
  stringstream ss1("012");
  stream1 << ss1.rdbuf();
  stream1.seekg(0, std::ios_base::beg);
  StreamBuf *streambuf1 = dynamic_cast<StreamBuf *>(stream1.rdbuf());
  vector<char> buf0_;
  buf0_.reserve(2);
  buf0_.push_back('0');
  buf0_.push_back('1');
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(streambuf1)->GetBuffer()) ==
              buf0_);
}

TEST(IOStreamTest, Write2) {
  Buffer buf(new vector<char>(3));
  IOStream stream(buf, 3);
  stringstream ss("012");
  stream.seekp(1, std::ios_base::beg);
  stream << ss.rdbuf();
  stream.seekg(0, std::ios_base::beg);
  StreamBuf *streambuf = dynamic_cast<StreamBuf *>(stream.rdbuf());
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back(static_cast<char>(0));
  buf0.push_back('0');
  buf0.push_back('1');
  EXPECT_TRUE(*(const_cast<const StreamBuf *>(streambuf)->GetBuffer()) == buf0);
}

TEST(StreamUtilsTest, Default) {
  vector<char> buf0;
  buf0.reserve(3);
  buf0.push_back('0');
  buf0.push_back('1');
  buf0.push_back('2');
  Buffer buf(new vector<char>(buf0));

  boost::shared_ptr<IOStream> stream =
      boost::make_shared<IOStream>(buf, 2);
  EXPECT_EQ(StreamUtils::GetStreamSize(stream), 2u);
  EXPECT_EQ(StreamUtils::GetStreamInputSize(stream), 2u);
  EXPECT_EQ(StreamUtils::GetStreamOutputSize(stream), 2u);
}

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
