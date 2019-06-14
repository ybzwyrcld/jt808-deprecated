#ifndef JT808_SERVICE_JT808_VEHICLE_CONTROL_H_
#define JT808_SERVICE_JT808_VEHICLE_CONTROL_H_

#include <stdint.h>


union VehicleControlFlag {
  struct Bit {
    uint8_t doorlock:1;  // 0:车门解锁; 1: 车门加锁
    uint8_t retain:7;
  }bit;
  uint8_t value;
};

#endif // JT808_SERVICE_JT808_VEHICLE_CONTROL_H_
