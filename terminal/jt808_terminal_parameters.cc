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

#include "terminal/jt808_terminal_parameters.h"

#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <string>
#include <utility>

#include "bcd/bcd.h"
#include "common/jt808_util.h"
#include "common/jt808_terminal_parameters.h"


int ReadTerminalParameterFormFile(const char *path,
                                  std::map<uint32_t, std::string> *map) {
  int retval = -1;

  system("sync");
  system("sync");
  std::ifstream ifs;
  ifs.open(path, std::ios::in | std::ios::binary);
  if (ifs.is_open()) {
    std::string str;
    uint32_t u32val;
    char value[256] = {0};
    while (getline(ifs, str)) {
      if (str.empty()) break;
      memset(value, 0x0, sizeof (value));
      sscanf(str.c_str(), "%x:%[^;]", &u32val, value);
      map->insert(std::make_pair(u32val, value));
    }
    ifs.close();
    if (!map->empty()) retval = 0;
  }
  return retval;
}


int WriteTerminalParameterToFile(const char *path,
                                 const std::map<uint32_t, std::string> &map) {
  int retval = -1;

  if (map.empty())  return retval;

  std::ofstream ofs;
  ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (ofs.is_open()) {
    char line[512] = {0};
    int len = 0;
    for (auto parameter : map) {
      memset(line, 0x0, sizeof (line));
      len = snprintf(line, sizeof (line), "%04X:%s;\n",
                     parameter.first, parameter.second.c_str());
      ofs.write(line, len);
    }
    ofs.close();
    system("sync");
    system("sync");
    retval = 0;
  }
  return retval;
}

int PrepareTerminalParameterIdList(const uint8_t *buf, const size_t &size,
                                   const std::map<uint32_t, std::string> &map,
                                   std::vector<uint32_t> *id_list) {
  uint32_t parameter_id = 0;

  for (size_t i = 0; i < size; ++i) {
    memcpy(&parameter_id, buf + i * 4, 4);
    parameter_id = EndianSwap32(parameter_id);
    try {
      map.at(parameter_id);
      id_list->push_back(parameter_id);
    } catch(const std::out_of_range) {
      fprintf(stderr, "No such terminal parameter id!!!\n");
      return -1;
    }
  }

  return 0;
}

void SetTerminalParameterValue(const uint32_t &id, const char *value,
                               std::map<uint32_t, std::string> *map) {
    uint8_t u8val = 0;
    uint16_t u16val = 0;
    uint32_t u32val = 0;
    for (auto &parameter : *map) {
      if (parameter.first == id) {
        switch (GetParameterTypeByParameterId(id)) {
          case kByteType:
            u8val = static_cast<uint8_t>(*value);
            parameter.second = std::to_string(u8val);
            break;
          case kWordType:
            memcpy(&u16val, &value[0], 2);
            u16val = EndianSwap16(u16val);
            parameter.second = std::to_string(u16val);
            break;
          case kDwordType:
            memcpy(&u32val, &value[0], 4);
            u32val = EndianSwap32(u32val);
            parameter.second = std::to_string(u32val);
            break;
          case kStringType:
            parameter.second = value;
            break;
          default:
            printf("unknow type\n");
            break;
        }
        break;
      }
    }
}
