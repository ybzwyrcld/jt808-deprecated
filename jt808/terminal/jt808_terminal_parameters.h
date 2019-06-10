#ifndef JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
#define JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_

#include <stdint.h>

#include <list>

#include "common/jt808_terminal_parameters.h"


void ReadTerminalParameterFormFile(const char *path,
                                   std::list<TerminalParameter> &list);
void WriteTerminalParameterToFile(const char *path,
                                  const std::list<TerminalParameter> &list);

#endif // JT808_TERMINAL_JT808_TERMINAL_PARAMETERS_H_
