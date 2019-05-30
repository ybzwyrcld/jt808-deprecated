#ifndef JT808_SERVICE_JT808_UTIL_H_
#define JT808_SERVICE_JT808_UTIL_H_

#include <assert.h>
#include <string.h>

#include <string>
#include <list>
#include <vector>

#include "bcd/bcd.h"

struct DeviceNode {
  bool has_upgrade;
  bool upgrading;
  char phone_num[12];
  char authen_code[8];
  char manufacturer_id[5];
  char upgrade_version[12];
  char upgrade_type;
  char file_path[256];
  int socket_fd;
};

int EpollRegister(const int &epoll_fd, const int &fd);
int EpollUnregister(const int &epoll_fd, const int &fd);

static inline uint16_t EndianSwap16(const uint16_t &value) {
  assert(sizeof(value) == 2);
  return (((value & 0xff) << 8) | (value >> 8));
}

static inline uint32_t EndianSwap32(const uint32_t &value) {
  assert(sizeof(value) == 4);
  return ((value >> 24) |
          ((value & 0x00ff0000) >> 8) |
          ((value & 0x0000ff00) << 8) |
          (value << 24));
}

uint8_t BccCheckSum(const uint8_t *src, const int &len);
uint16_t Escape(uint8_t *src, const int &len);
uint16_t ReverseEscape(uint8_t *src, const int &len);

static inline void PreparePhoneNum(const char *src, uint8_t *bcd_array) {
  char phone_num[6] = {0};
  BcdFromStringCompress(src, phone_num, strlen(src));
  memcpy(bcd_array, phone_num, 6);
}

void ReadDevicesList(const char *path, std::list<DeviceNode> &list);
int SearchStringInList(const std::vector<std::string> &va_vec,
                       const std::string &str);

#endif  // JT808_SERVICE_JT808_UTIL_H_
