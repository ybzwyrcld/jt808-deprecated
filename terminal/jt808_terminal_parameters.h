#ifndef JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
#define JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_

#include <stdint.h>

#include <list>
#include <map>
#include <string>

#include "common/jt808_terminal_parameters.h"


int ReadTerminalParameterFormFile(const char *path,
                                  std::map<uint32_t, std::string> &map);
int WriteTerminalParameterToFile(const char *path,
                                 std::map<uint32_t, std::string> &map);
int PrepareTerminalParameterIdList(const uint8_t *buf, const size_t &size,
                                   const std::map<uint32_t, std::string> &map,
                                   std::vector<uint32_t> *id_list);

#endif // JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
