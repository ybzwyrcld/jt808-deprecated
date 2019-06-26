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

#include "terminal/jt808_terminal.h"


using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Ne;

class Jt808TerminalTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Jt808Info jt808_info = {"127.0.0.1", 8193, "13826539850", 2};
    terminal_.set_jt808_info(jt808_info);
  }

  // virtual void TearDown() {}

  Jt808Terminal terminal_;
};

TEST_F(Jt808TerminalTest, SetJt808InfoTest) {
  Jt808Info jt808_info = {"127.0.0.1", 8193, "13826539850", 2};
  Jt808Info jt808_info_read = terminal_.jt808_info();
  EXPECT_THAT(memcmp(&jt808_info_read, &jt808_info, sizeof(jt808_info)), Eq(0));
}

TEST_F(Jt808TerminalTest, ConnectRemoteServerTest) {
  EXPECT_THAT(terminal_.ConnectRemote(), Eq(0));
}

TEST_F(Jt808TerminalTest, DisconnectServerTest) {
  EXPECT_THAT(terminal_.is_connect(), IsFalse());
  EXPECT_THAT(terminal_.ConnectRemote(), Eq(0));
  EXPECT_THAT(terminal_.is_connect(), IsTrue());
  terminal_.ClearConnect();
  EXPECT_THAT(terminal_.is_connect(), IsFalse());
}

