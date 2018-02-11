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

#include <string>

#include "gtest/gtest.h"

#include "base/Exception.h"

namespace {

using QS::Exception::QSException;
using std::string;

static const char *const testMsg = "test QSException";

void ThrowException() { throw QSException(testMsg); }

string GetExceptionMsg() {
  try {
    ThrowException();
  } catch (const QSException &err) {
    return err.get();
  }
  return string();
}

}  // namespace

TEST(QSExceptionTest, DefaultTest) {
  EXPECT_EQ(GetExceptionMsg(), string(testMsg));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
