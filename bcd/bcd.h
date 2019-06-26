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

#ifndef JT808_BCD_BCD_H_
#define JT808_BCD_BCD_H_

#include <stdint.h>
#include <string.h>


uint8_t BcdFromHex(const uint8_t &src);
uint8_t HexFromBcd(const uint8_t &src);
char *BcdFromStringCompress(const char *src, char *dst, const size_t &srclen);
char *StringFromBcdCompress(const char *src, char *dst, const size_t &srclen);
char *StringFromBcdCompressFillingZero(const char *src,
                                       char *dst, const int &srclen);

#endif  // JT808_BCD_BCD_H_
