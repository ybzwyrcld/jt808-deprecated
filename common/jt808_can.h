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

#endif // JT808_COMMON_JT808_CAN_H_
