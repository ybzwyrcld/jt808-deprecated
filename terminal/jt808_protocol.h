#ifndef JT808_TERMINAL_JT808_PROTOCOL_H_
#define JT808_TERMINAL_JT808_PROTOCOL_H_

#include <stdint.h>

#include <list>
#include <map>
#include <vector>

#include "common/jt808_protocol.h"
#include "common/jt808_vehicle_control.h"


#pragma pack(push, 1)

// 协议参数
struct ProtocolParameters {
  uint8_t respond_result;
  uint8_t respond_para_num;
  uint8_t upgrade_type;
  uint16_t respond_flow_num;
  uint16_t respond_id;
  uint16_t packet_first_flow_num;
  uint16_t packet_total_num;
  uint16_t packet_sequence_num;
  uint32_t packet_data_len;
  uint8_t packet_data[1024];
  VehicleControlFlag vehicle_control_flag;
  std::map<uint16_t, Message> *packet_map;
  std::list<uint16_t> *packet_id_list;
  std::list<TerminalParameter *> *terminal_parameter_list;
  std::vector<uint32_t> *terminal_parameter_id_list;
};

#pragma pack(pop)

#endif // JT808_TERMINAL_JT808_PROTOCOL_H_
