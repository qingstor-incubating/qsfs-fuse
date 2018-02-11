// +-------------------------------------------------------------------------
// | Copyright (C) 2018 Yunify, Inc.
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

#include "base/LogLevel.h"

using std::string;

TEST(LogLevelTest, LogLevelName) {
  using namespace QS::Logging;  // NOLINT
  EXPECT_EQ(string("INFO"), GetLogLevelName(LogLevel::Info));
  EXPECT_EQ(string("WARN"), GetLogLevelName(LogLevel::Warn));
  EXPECT_EQ(string("ERROR"), GetLogLevelName(LogLevel::Error));
  EXPECT_EQ(string("FATAL"), GetLogLevelName(LogLevel::Fatal));
}

TEST(LogLevelTest, LogLevelByName) {
  using namespace QS::Logging;  // NOLINT;
  EXPECT_EQ(LogLevel::Info, GetLogLevelByName("info"));
  EXPECT_EQ(LogLevel::Warn, GetLogLevelByName("warn"));
  EXPECT_EQ(LogLevel::Warn, GetLogLevelByName("warning"));
  EXPECT_EQ(LogLevel::Error, GetLogLevelByName("error"));
  EXPECT_EQ(LogLevel::Fatal, GetLogLevelByName("fatal"));

  EXPECT_EQ(LogLevel::Info, GetLogLevelByName("INFO"));
  EXPECT_EQ(LogLevel::Warn, GetLogLevelByName("WARN"));
  EXPECT_EQ(LogLevel::Warn, GetLogLevelByName("WARNING"));
  EXPECT_EQ(LogLevel::Error, GetLogLevelByName("ERROR"));
  EXPECT_EQ(LogLevel::Fatal, GetLogLevelByName("FATAL"));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
