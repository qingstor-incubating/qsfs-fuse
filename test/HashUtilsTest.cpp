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

#include "boost/unordered_map.hpp"
#include "gtest/gtest.h"

#include "base/HashUtils.h"

using boost::unordered_map;
using std::string;

struct Color {
  enum Value { White, Red, Black, NIL };
};

TEST(HashUtilsTest, EnumHash) {
  unordered_map<Color::Value, string, QS::HashUtils::EnumHash> mapColor;
  mapColor[Color::White] = "white";
  mapColor[Color::Red] = "red";
  mapColor[Color::Black] = "black";

  EXPECT_EQ(mapColor.at(Color::Red), "red");
  EXPECT_EQ(mapColor.find(Color::NIL), mapColor.end());
}

TEST(HashUtilsTest, StringHash) {
  static const char *const AP_1 = "ap1";
  static const char *const PEK_2 = "pek2";
  static const char *const PEK_3A = "pek3a";
  unordered_map<string, string, QS::HashUtils::StringHash> mapZone;
  mapZone[AP_1] = "ap1.qingstor.com";
  mapZone[PEK_2] = "pek2.qingstor.com";
  mapZone[PEK_3A] = "pek3a.qingstor.com";

  EXPECT_EQ(mapZone.at(AP_1), "ap1.qingstor.com");
  EXPECT_EQ(mapZone.find("missing"), mapZone.end());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
