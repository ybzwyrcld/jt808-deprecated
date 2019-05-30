#include "service/jt808_position_report.h"

#include <unistd.h>
#include <string.h>

#include "service/jt808_util.h"


void ParsePositionReport(const uint8_t *buffer, const int len) {
  uint16_t u16val;
  uint32_t u32val;
  double latitude;
  double longitude;
  float altitude;
  float speed;
  float bearing;
  AlarmBit alarm_bit = {0};
  StatusBit status_bit = {0};
  char timestamp[6];
  char phone_num[6] = {0};
  char device_num[12] = {0};

  memcpy(phone_num, &buffer[5], 6);
  StringFromBcdCompress(phone_num, device_num, 6);
  memcpy(&u32val, &buffer[13], 4);
  alarm_bit.value = EndianSwap32(u32val);
  memcpy(&u32val, &buffer[17], 4);
  status_bit.value = EndianSwap32(u32val);
  memcpy(&u32val, &buffer[21], 4);
  latitude = EndianSwap32(u32val) / 1000000.0;
  memcpy(&u32val, &buffer[25], 4);
  longitude = EndianSwap32(u32val) / 1000000.0;
  memcpy(&u16val, &buffer[29], 2);
  altitude = EndianSwap16(u16val);
  memcpy(&u16val, &buffer[31], 2);
  speed = EndianSwap16(u16val) * 10.0;
  memcpy(&u16val, &buffer[33], 2);
  bearing = EndianSwap16(u16val);
  timestamp[0] = HexFromBcd(buffer[35]);
  timestamp[1] = HexFromBcd(buffer[36]);
  timestamp[2] = HexFromBcd(buffer[37]);
  timestamp[3] = HexFromBcd(buffer[38]);
  timestamp[4] = HexFromBcd(buffer[39]);
  timestamp[5] = HexFromBcd(buffer[40]);
  fprintf(stdout, "\tdevice: %s\n"
                  "\talarm flags: %08X\n""\tstatus flags: %08X\n"
                  "\tlongitude: %lf%c\n"
                  "\tlatitude: %lf%c\n"
                  "\taltitude: %f\n"
                  "\tspeed: %f\n""\tbearing: %f\n"
                  "\ttimestamp: 20%02d-%02d-%02d, %02d:%02d:%02d\n",
          device_num, alarm_bit.value, status_bit.value,
          longitude, status_bit.bit.ewlongitude == 0 ? 'E':'W',
          latitude, status_bit.bit.snlatitude == 0 ? 'N':'S',
          altitude,
          speed, bearing,
          timestamp[0], timestamp[1], timestamp[2],
          timestamp[3], timestamp[4], timestamp[5]);
  if (len >= 46) {
    fprintf(stdout, "\tgnss satellite count: %d\n", buffer[43]);
  }
  if (len >= 51) {
    fprintf(stdout, "\tgnss position status: %d\n", buffer[48]);
  }
}

