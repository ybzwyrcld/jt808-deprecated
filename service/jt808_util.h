#ifndef JT808_SERVICE_JT808_UTIL_H_
#define JT808_SERVICE_JT808_UTIL_H_

#include <string>
#include <list>
#include <vector>

#include "common/jt808_util.h"
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
bool ReadDevicesList(const char *path, std::list<DeviceNode> &list);
int SearchStringInList(const std::vector<std::string> &va_vec,
                       const std::string &str);

#endif  // JT808_SERVICE_JT808_UTIL_H_
