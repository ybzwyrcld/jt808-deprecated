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

#ifndef JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
#define JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_

#include <stdint.h>

#include <string>
#include <list>
#include <map>
#include <vector>


int ReadTerminalParameterFormFile(const char *path,
                                  std::map<uint32_t, std::string> *map);
int WriteTerminalParameterToFile(const char *path,
                                 const std::map<uint32_t, std::string> &map);
int PrepareTerminalParameterIdList(const uint8_t *buf, const size_t &size,
                                   const std::map<uint32_t, std::string> &map,
                                   std::vector<uint32_t> *id_list);
void SetTerminalParameterValue(const uint32_t &id, const char *value,
                               std::map<uint32_t, std::string> *map);

#endif  // JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
