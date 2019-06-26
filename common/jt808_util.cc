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

#include "common/jt808_util.h"

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcd/bcd.h"
#include "common/jt808_protocol.h"


uint16_t EndianSwap16(const uint16_t &value) {
  assert(sizeof(value) == 2);
  return static_cast<uint16_t>(((value & 0xff) << 8) | (value >> 8));
}

uint32_t EndianSwap32(const uint32_t &value) {
  assert(sizeof(value) == 4);
  return ((value >> 24) |
          ((value & 0x00ff0000) >> 8) |
          ((value & 0x0000ff00) << 8) |
          (value << 24));
}

uint8_t BccCheckSum(const uint8_t *src, const size_t &len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; ++i) {
    checksum = checksum ^ src[i];
  }
  return checksum;
}

size_t Escape(uint8_t *src, const size_t &len) {
  size_t i;
  size_t j;
  uint8_t *buffer = new uint8_t[len * 2];

  memset(buffer, 0x0, len * 2);
  for (i = 0, j = 0; i < len; ++i) {
    if (src[i] == PROTOCOL_SIGN) {
      buffer[j++] = PROTOCOL_ESCAPE;
      buffer[j++] = PROTOCOL_ESCAPE_SIGN;
    } else if (src[i] == PROTOCOL_ESCAPE) {
      buffer[j++] = PROTOCOL_ESCAPE;
      buffer[j++] = PROTOCOL_ESCAPE_ESCAPE;
    } else {
      buffer[j++] = src[i];
    }
  }

  memcpy(src, buffer, j);
  delete [] buffer;
  return j;
}

size_t ReverseEscape(uint8_t *src, const size_t &len) {
  size_t i;
  size_t j;
  uint8_t *buffer = new uint8_t[len];

  memset(buffer, 0x0, len);
  for (i = 0, j = 0; i < len; ++i) {
    if ((src[i] == PROTOCOL_ESCAPE) && (src[i+1] == PROTOCOL_ESCAPE_SIGN)) {
      buffer[j++] = PROTOCOL_SIGN;
      ++i;
    } else if ((src[i] == PROTOCOL_ESCAPE) &&
               (src[i+1] == PROTOCOL_ESCAPE_ESCAPE)) {
      buffer[j++] = PROTOCOL_ESCAPE;
      ++i;
    } else {
      buffer[j++] = src[i];
    }
  }

  memcpy(src, buffer, j);
  delete [] buffer;
  return j;
}

void PreparePhoneNum(const char *src, uint8_t *bcd_array) {
  char phone_num[6] = {0};
  BcdFromStringCompress(src, phone_num, strlen(src));
  memcpy(bcd_array, phone_num, 6);
}
