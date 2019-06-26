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

#ifndef JT808_COMMON_JT808_CAN_H_
#define JT808_COMMON_JT808_CAN_H_

#include <stdint.h>

#pragma pack(push, 1)

union CanIdBit {
  struct Bit {
    uint32_t id:29;
    uint32_t collectmethod:1;
    uint32_t frametype:1;
    uint32_t channel:1;
  }bit;
  uint32_t value;
};

struct CanBusDataTimestamp {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
};

struct CanBusData {
  CanIdBit can_id;
  uint8_t buffer[8];
};

#pragma pack(pop)

#endif  // JT808_COMMON_JT808_CAN_H_
