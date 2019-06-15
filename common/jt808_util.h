#ifndef JT808_COMMAN_JT808_UTIL_H_
#define JT808_COMMON_JT808_UTIL_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "bcd/bcd.h"


static inline uint16_t EndianSwap16(const uint16_t &value) {
  assert(sizeof(value) == 2);
  return static_cast<uint16_t>(((value & 0xff) << 8) | (value >> 8));
}

static inline uint32_t EndianSwap32(const uint32_t &value) {
  assert(sizeof(value) == 4);
  return ((value >> 24) |
          ((value & 0x00ff0000) >> 8) |
          ((value & 0x0000ff00) << 8) |
          (value << 24));
}

uint8_t BccCheckSum(const uint8_t *src, const size_t &len);
size_t Escape(uint8_t *src, const size_t &len);
size_t ReverseEscape(uint8_t *src, const size_t &len);

static inline void PreparePhoneNum(const char *src, uint8_t *bcd_array) {
  char phone_num[6] = {0};
  BcdFromStringCompress(src, phone_num, strlen(src));
  memcpy(bcd_array, phone_num, 6);
}

#endif  // JT808_COMMAN_JT808_UTIL_H_
