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

#ifndef JT808_COMMON_JT808_VEHICLE_CONTROL_H_
#define JT808_COMMON_JT808_VEHICLE_CONTROL_H_

#include <stdint.h>


union VehicleControlFlag {
  struct Bit {
    uint8_t doorlock:1;  // 0:车门解锁; 1: 车门加锁
    uint8_t retain:7;
  }bit;
  uint8_t value;
};

#endif  // JT808_COMMON_JT808_VEHICLE_CONTROL_H_
