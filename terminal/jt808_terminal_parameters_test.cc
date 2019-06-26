// Copyright 2019 Yuming Meng. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "terminal/jt808_terminal_parameters.h"


using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Ne;

static bool MapCommpare(const std::map<uint32_t, std::string> &map1,
                        const std::map<uint32_t, std::string> &map2) {
  if (map1.size() != map2.size()) return false;
  auto map1_it = map1.begin();
  auto map2_it = map2.begin();
  while (1) {
    if ((map1_it->first != map2_it->first) ||
        (map1_it->second != map2_it->second)) {
      return false;
    }
    ++map1_it;
    ++map2_it;
    if (map1_it == map1.end()) break;
  }
  return true;
}

TEST(Jt808TerminalParameterTest, ReadWriteTerminalParameterTest) {
  const char *path = "/etc/jt808/terminal/terminalparameter.txt\0";
  std::map<uint32_t, std::string> map1;
  std::map<uint32_t, std::string> map2;

  EXPECT_THAT(ReadTerminalParameterFormFile(path, &map1), Eq(0));
  EXPECT_THAT(WriteTerminalParameterToFile(path, map1), Eq(0));
  EXPECT_THAT(ReadTerminalParameterFormFile(path, &map2), Eq(0));
  EXPECT_THAT(MapCommpare(map1, map2), IsTrue());
  // for (auto parameter: map1) {
  //   printf("%X, %s\n", parameter.first, parameter.second.c_str());
  // }
}

