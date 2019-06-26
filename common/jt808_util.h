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

#ifndef JT808_COMMON_JT808_UTIL_H_
#define JT808_COMMON_JT808_UTIL_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>


uint16_t EndianSwap16(const uint16_t &value);
uint32_t EndianSwap32(const uint32_t &value);
uint8_t BccCheckSum(const uint8_t *src, const size_t &len);
size_t Escape(uint8_t *src, const size_t &len);
size_t ReverseEscape(uint8_t *src, const size_t &len);
void PreparePhoneNum(const char *src, uint8_t *bcd_array);

#endif  // JT808_COMMON_JT808_UTIL_H_
