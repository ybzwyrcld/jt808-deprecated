#include "service/jt808_util.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <time.h>

#include <iostream>
#include <fstream>

#include "service/jt808_protocol.h"


int EpollRegister(const int &epoll_fd, const int &fd) {
  struct epoll_event ev;
  int ret;
  int flags;

  // important: make the fd non-blocking.
  flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  ev.events = EPOLLIN;
  //ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = fd;
  do {
      ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

int EpollUnregister(const int &epoll_fd, const int &fd) {
  int ret;

  do {
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

uint8_t BccCheckSum(const uint8_t *src, const int &len) {
  uint8_t checksum = 0;
  for (int i = 0; i < len; ++i) {
    checksum = checksum ^ src[i];
  }
  return checksum;
}

uint16_t Escape(uint8_t *src, const int &len) {
  uint16_t i;
  uint16_t j;
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

uint16_t ReverseEscape(uint8_t *src, const int &len) {
  uint16_t i;
  uint16_t j;
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

void ReadDevicesList(const char *path, std::list<DeviceNode> &list) {
  char *result;
  char line[128] = {0};
  char flags = ';';
  uint32_t u32val;
  std::ifstream ifs;

  ifs.open(path, std::ios::in | std::ios::binary);
  if (ifs.is_open()) {
    std::string str;
    while (getline(ifs, str)) {
      DeviceNode node = {0};
      memset(line, 0x0, sizeof(line));
      str.copy(line, str.length(), 0);
      result = strtok(line, &flags);
      str = result;
      str.copy(node.phone_num, str.size(), 0);
      result = strtok(NULL, &flags);
      str = result;
      u32val = 0;
      sscanf(str.c_str(), "%u", &u32val);
      memcpy(node.authen_code, &u32val, 4);
      node.socket_fd = -1;
      list.push_back(node);
    }
    ifs.close();
  }
}

int SearchStringInList(const std::vector<std::string> &va_vec,
                       const std::string &arg) {
  auto va_it = va_vec.begin();
  while (va_it != va_vec.end()) {
    if (*va_it == arg) {
      break;
    }
    ++va_it;
  }
  return (va_it == va_vec.end() ? 0 : 1);
}

