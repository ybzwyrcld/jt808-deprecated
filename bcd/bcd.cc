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

#include "bcd/bcd.h"

#include <stdint.h>


unsigned char BcdFromHex(const uint8_t &src) {
  unsigned char temp;

  temp = ((src / 10) << 4) + (src % 10);

  return temp;
}

unsigned char HexFromBcd(const uint8_t &src) {
  unsigned char temp;

  temp = (src >> 4)*10 + (src & 0x0f);
  return temp;
}

char *BcdFromStringCompress(const char *src, char *dst, const size_t &srclen) {
  char *ptr = dst;
  unsigned char temp;

  if (srclen % 2 != 0) {
    *ptr++ = BcdFromHex(*src++ - '0');
  }

  while (*src) {
    // *ptr++ = hex2bcd((*src++ - '0')*10 + (*src++ - '0'));
    temp = *src++ - '0';
    temp *= 10;
    temp += *src++ - '0';
    *ptr++ = BcdFromHex(temp);
  }

  *ptr = 0;

  return dst;
}

char *StringFromBcdCompress(const char *src, char *dst, const size_t &srclen) {
  char *ptr = dst;
  unsigned char temp;
  int cnt = srclen;

  while (cnt--) {
    temp = HexFromBcd(*src);
    *ptr++ = temp/10 + '0';
    if (dst[0] == '0')
      ptr = dst;
    *ptr++ = temp%10 + '0';
    ++src;
  }

  return dst;
}

char *StringFromBcdCompressFillingZero(const char *src,
                                       char *dst, const int &srclen) {
  char *ptr = dst;
  char temp;
  int cnt = srclen;

  while (cnt--) {
    temp = HexFromBcd(*src);
    *ptr++ = temp/10 + '0';
    *ptr++ = temp%10 + '0';
    ++src;
  }
  return dst;
}
