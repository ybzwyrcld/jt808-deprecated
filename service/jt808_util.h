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

#ifndef JT808_SERVICE_JT808_UTIL_H_
#define JT808_SERVICE_JT808_UTIL_H_

#include <string>
#include <list>
#include <vector>


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
bool ReadDevicesList(const char *path, std::list<DeviceNode *> *list);
int SearchStringInList(const std::vector<std::string> &va_vec,
                       const std::string &str);

#endif  // JT808_SERVICE_JT808_UTIL_H_
