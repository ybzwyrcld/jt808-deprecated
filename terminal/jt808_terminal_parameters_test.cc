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

  EXPECT_THAT(ReadTerminalParameterFormFile(path, map1), Eq(0));
  EXPECT_THAT(WriteTerminalParameterToFile(path, map1), Eq(0));
  EXPECT_THAT(ReadTerminalParameterFormFile(path, map2), Eq(0));
  EXPECT_THAT(MapCommpare(map1, map2), IsTrue());
  // for (auto parameter: map1) {
  //   printf("%X, %s\n", parameter.first, parameter.second.c_str());
  // }
}

