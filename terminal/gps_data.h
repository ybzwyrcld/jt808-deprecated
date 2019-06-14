#ifndef JT808_TERMINAL_GPS_DATA_H_
#define JT808_TERMINAL_GPS_DATA_H_

struct PositionInfo {
  double latitude;
  double longitude;
  double altitude;
  float speed;
  float bearing;
  char timestamp[6];
};

#endif // JT808_TERMINAL_GPS_DATA_H_
