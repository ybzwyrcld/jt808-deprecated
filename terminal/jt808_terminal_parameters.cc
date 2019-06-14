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

#include "bcd/bcd.h"
#include "common/jt808_util.h"


int ReadTerminalParameterFormFile(const char *path,
                                  std::map<uint32_t, std::string> &map) {
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
      memset(value, 0x0, 256);
      sscanf(str.c_str(), "%x:%[^;]", &u32val, value);
      map.insert(std::make_pair(u32val, value));
    }
    ifs.close();
    if (!map.empty()) retval = 0;
  }
  return retval;
}


int WriteTerminalParameterToFile(const char *path,
                                 std::map<uint32_t, std::string> &map) {
  int retval = -1;

  if (map.empty())  return retval;

  std::ofstream ofs;
  ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (ofs.is_open()) {
    char line[512] = {0};
    int len = 0;
    for (auto parameter : map) {
      memset(line, 0x0, 512);
      len = snprintf(line, 512, "%04X:%s;\n",
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
    } catch(const std::out_of_range& e) {
      fprintf(stderr, "No such terminal parameter id!!!\n");
      return -1;
    }
  }

  return 0;
}
