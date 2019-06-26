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

#ifndef JT808_COMMON_JT808_PASSTHROUGH_H_
#define JT808_COMMON_JT808_PASSTHROUGH_H_

#include <stdint.h>


// GNSS 模块详细定位数据
#define PASSTHROUGH_GNSSRAW      0x00
// 道路运输证 IC 卡信息,
// 上传消息为 64Byt, 下传消息为 24Byte
#define PASSTHROUGH_ROADICCARD   0x0B
// 串口 1 透传消息
#define PASSTHROUGH_SERIALPORT1  0x41
// 串口 2 透传消息
#define PASSTHROUGH_SERIALPORT2  0x42

// 用户自定义透传消息 0xF0-0xFF

#pragma pack(push, 1)

struct PassThrough {
  uint8_t type;
  uint8_t buffer[1023];
  uint32_t size;
};

#pragma pack(pop)

#endif  // JT808_COMMON_JT808_PASSTHROUGH_H_
