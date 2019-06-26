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

#ifndef JT808_COMMON_JT808_TERMINAL_CONTROL_H_
#define JT808_COMMON_JT808_TERMINAL_CONTROL_H_


#define WIRELESSUPGRADE                     0x01
#define CONNECTSPECIALSERVER                0x02
#define TERMINALPOWEROFF                    0x03
#define TERMINALREBOOT                      0x04
#define TERMINALRESET                       0x05
#define TURNOFFDATACOMMUNICATION            0x06
#define TURNOFFALLWIRELESSCOMMUNICATION     0x07

enum TerminalControlType {
  kUnknowControlType = 0x0,
  kWirelessUpgrade,
  kConnectSpecialServer,
  kTerminalPowerOff,
  kTerminalReboot,
  kTerminalReset,
  kTurnOffDataCommunication,
  kTurnOffAllWirelessCommunication,
};

#endif  // JT808_COMMON_JT808_TERMINAL_CONTROL_H_
