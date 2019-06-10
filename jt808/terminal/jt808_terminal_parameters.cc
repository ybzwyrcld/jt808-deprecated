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


void ReadTerminalParameterFormFile(const char *path,
                                   std::list<TerminalParameter> &list) {
  char *result;
  char line[512] = {0};
  char flags = ';';
  uint32_t u32val;

  system("sync");
  system("sync");
  std::ifstream ifs;
  ifs.open(path, std::ios::in | std::ios::binary);
  if (ifs.is_open()) {
    std::string str;
    uint8_t type;
    while (getline(ifs, str)) {
      // one line: 'id;len(in bytes);value'.
      TerminalParameter node;
      memset(line, 0x0, sizeof(line));
      memset(&node, 0x0, sizeof(node));
      str.copy(line, str.length(), 0);
      result = strtok(line, &flags);
      sscanf(result, "%x", &u32val);
      node.parameter_id = u32val;
      type = GetParameterTypeByParameterId(node.parameter_id);
      result = strtok(NULL, &flags);
      sscanf(result, "%u", &u32val);
      node.parameter_len = static_cast<uint8_t>(u32val);
      result = strtok(NULL, &flags);
      if (type == kStringType) {
        str = result;
        memcpy(node.parameter_value, str.c_str(), str.length());
      } else {
        sscanf(result, "%u", &u32val);
        memcpy(node.parameter_value, &u32val, node.parameter_len);
      }
      list.push_back(node);
    }
    ifs.close();
  }
}

void WriteTerminalParameterToFile(const char *path,
                                  const std::list<TerminalParameter> &list) {
  char line[512] = {0};

  if (list.empty()) {
    return ;
  }

  std::ofstream ofs;
  ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (ofs.is_open()) {
    uint8_t type;
    uint32_t u32val;
    auto it = list.begin();
    while (it != list.end()) {
      memset(line, 0x0, sizeof(line));
      type = GetParameterTypeByParameterId(it->parameter_id);
      if (type == kStringType) {
        snprintf(line, sizeof(line),
                "%04X;%u;%s;\n",
                it->parameter_id,
                it->parameter_len,
                it->parameter_value);
      } else {
        u32val = 0;
        memcpy(&u32val, it->parameter_value, it->parameter_len);
        snprintf(line, sizeof(line),
                "%04X;%u;%u;\n",
                it->parameter_id,
                it->parameter_len,
                it->parameter_len == 4 ? u32val:u32val);
      }
      //printf("line: %s\n", line);
      ofs.write(line, strlen(line));
      ++it;
    }
    ofs.close();
    system("sync");
    system("sync");
  }
}

