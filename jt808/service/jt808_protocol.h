#ifndef JT808_SERVICE_JT808_PROTOCOL_H_
#define JT808_SERVICE_JT808_PROTOCOL_H_

#include <stdint.h>
#include <unistd.h>

#include <list>
#include <map>

#include "common/jt808_protocol.h"
#include "common/jt808_vehicle_control.h"
#include "service/jt808_position_report.h"


#pragma pack(push, 1)

// 协议参数
struct ProtocolParameters {
  uint8_t respond_result;
  uint8_t respond_para_num;
  uint8_t version_num_len;
  uint8_t upgrade_type;
  uint8_t set_area_route_type;
  uint8_t terminal_parameter_id_count;
  uint8_t area_route_id_count;
  uint16_t respond_flow_num;
  uint16_t respond_id;
  uint16_t packet_first_flow_num;
  uint16_t packet_response_success_num;
  uint16_t packet_total_num;
  uint16_t packet_sequence_num;
  uint32_t packet_data_len;
  uint8_t manufacturer_id[5];
  uint8_t phone_num[6];
  uint8_t authen_code[4];
  uint8_t version_num[32];
  uint8_t packet_data[1024];
  uint16_t report_interval;
  uint32_t report_valid_time;
  uint8_t terminal_control_type;
  VehicleControlFlag vehicle_control_flag;
  CanBusDataTimestamp can_bus_data_timestamp;
  PassThrough *pass_through;
  std::vector<CircularArea *> *circular_area_list;
  std::vector<RectangleArea *> *rectangle_area_list;
  std::vector<PolygonalArea *> *polygonal_area_list;
  std::vector<Route *> *route_list;
  std::vector<CanBusData *> *can_bus_data_list;
  std::list<uint16_t> *packet_id_list;
  std::map<uint16_t, Message> *packet_map;
  std::map<uint32_t, std::string> *terminal_parameter_map;
  uint8_t *terminal_parameter_id_buffer;
  uint8_t *area_route_id_buffer;
};

#pragma pack(pop)

#endif // JT808_SERVICE_JT808_PROTOCOL_H_
