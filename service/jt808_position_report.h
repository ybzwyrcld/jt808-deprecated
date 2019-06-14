#ifndef JT808_SERVICE_JT808_POSITION_REPORT_H_
#define JT808_SERVICE_JT808_POSITION_REPORT_H_

#include <stdint.h>

#include "common/jt808_position_report.h"


void ParsePositionReport(const uint8_t *buffer, const int &len, const int &offset);

#endif // JT808_SERVICE_JT808_POSITION_REPORT_H_
