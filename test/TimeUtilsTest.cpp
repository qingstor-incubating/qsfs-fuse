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

#include <string>

#include "gtest/gtest.h"

#include "base/TimeUtils.h"

using std::string;

TEST(TimeUtilsTest, TimeDateSwitch) {
  string epoch = "Thu, 01 Jan 1970 00:00:00 GMT";
  EXPECT_EQ(QS::TimeUtils::SecondsToRFC822GMT(0), epoch);
  EXPECT_EQ(QS::TimeUtils::RFC822GMTToSeconds(epoch), 0);

  time_t secondsOneHour = 3600;
  string oneHourSinceEpoch = "Thu, 01 Jan 1970 01:00:00 GMT";
  EXPECT_EQ(QS::TimeUtils::SecondsToRFC822GMT(secondsOneHour),
            oneHourSinceEpoch);
  EXPECT_EQ(QS::TimeUtils::RFC822GMTToSeconds(oneHourSinceEpoch),
            secondsOneHour);
}

TEST(TimeUtilsTest, IsExpire) {
  time_t epoch = 0;
  time_t now = time(NULL);
  int32_t oneDay = 60*12;

  using QS::TimeUtils::IsExpire;
  EXPECT_FALSE(IsExpire(now, oneDay));
  EXPECT_TRUE(IsExpire(epoch, oneDay));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
