#ifndef JT808_TERMINAL_JT808_TERMINAL_H_
#define JT808_TERMINAL_JT808_TERMINAL_H_

#include <unistd.h>

#include <string.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include "common/jt808_upgrade.h"
#include "terminal/gps_data.h"
#include "terminal/jt808_protocol.h"
#include "terminal/jt808_area_route.h"


struct Jt808Info {
  char server_ip[16];
  int server_port;
  char phone_number[12];
  int report_interval;
};

class Jt808Terminal {
 public:
  Jt808Terminal() = default;
  // Jt808Terminal is neither copyable nor movable.
  Jt808Terminal(const Jt808Terminal&) = delete;
  Jt808Terminal& operator=(const Jt808Terminal&) = delete;
  virtual ~Jt808Terminal();

  int Init(void);
  int RequestConnectServer(void);
  int RequestLoginServer(void);
  int ConnectRemote(void);
  int ReconnectRemote(void);
  void ClearConnect(void);
  int SendFrameData(void);
  int RecvFrameData(void);

  int Jt808FramePack(const uint16_t &commond);
  uint16_t Jt808FrameParse(void);
  int ReportPosition(void);
  void ReportUpgradeResult(void);

  // Alarm bit accessors/mutators.
  AlarmBit alarm_bit(void) const { return alarm_bit_; }
  void set_alarm_bit(const uint32_t &value) {
    alarm_bit_.value = value;
  }

  // Position status accessors/mutators.
  int positon_status(void) const { return position_status_.item_value[0]; }
  void set_position_status(const int &value) {
     position_status_.item_value[0] = value;
  }

  // Jt808 info accessors/mutators.
  Jt808Info jt808_info(void) const { return jt808_info_; }
  void set_jt808_info(const Jt808Info &jt808_info) {
    jt808_info_ = jt808_info;
  }

  // Status bit accessors/mutators.
  StatusBit status_bit(void) const { return status_bit_; }
  void set_status_bit(const uint32_t &value) {
    status_bit_.value = value;
  }

  // Passthrough body mutators.
  void set_pass_through(const PassThrough &pass_through) {
    pass_through_ = pass_through;
  }

  // set gps position info for commond 'UP_POSITIONREPORT'.
  void set_position_info(const PositionInfo &position_info) {
    position_info_ = position_info;
  }

  // special parameter set status accessors/mutators functions.
  int parameter_set_type(void) const { return parameter_set_type_; }
  void set_parameter_set_type(const int &parameter_set_type) {
    parameter_set_type_ = parameter_set_type;
  }

  // Can bus data timestamp mutators.
  void set_can_bus_data_timestamp(
           const CanBusDataTimestamp &can_bus_data_timestamp) {
    can_bus_data_timestamp_ = can_bus_data_timestamp;
  }

  // Can bus data mutators.
  void set_can_bus_data_list(std::vector<CanBusData> *can_bus_data_list) {
    can_bus_data_list_ = can_bus_data_list;
  }

  // gnss satellites num accessors/mutators.
  int gnss_satellite_num(void) const { return gnss_satellite_num_.item_value[0]; }
  void set_gnss_satellite_num(const int &value) {
     gnss_satellite_num_.item_value[0] = value;
  }

  // return current report interval.
  int report_interval(void) {
    if (report_valid_time_ >= 0) {
      report_valid_time_ -= report_interval_;
      return report_interval_;
    } else {
      return jt808_info_.report_interval;
    }
  }

  int heartbeat_interval(void) const {
    return atoi(terminal_parameter_map_.at(1).c_str());
  }

  int HeartBeat(void);

  int terminal_control_type(void) const { return terminal_control_type_; }

  // return current upgrade info.
  UpgradeInfo upgrade_info(void) const { return upgrade_info_; }

  // return socket connect status;
  bool is_connect(void) const { return is_connect_; }

  // return socket fd for communication;
  int socket_fd(void) const { return socket_fd_; }

 private:
  const char *kDownloadDir = "/upgrade";
  const char *kTerminalParametersFlie =
     "/etc/jt808/terminal/terminalparameter.txt";
  const char *kAreaRouteFlie = "/etc/jt808/terminal/jt808/arearoute.txt";

  bool is_connect_ = false;
  int socket_fd_ = -1;
  int message_flow_number_ = {0};
  int parameter_set_type_ = {0};
  int terminal_control_type_ = 0;
  int report_interval_ = -1;
  int64_t report_valid_time_ = -1;
  AlarmBit alarm_bit_;
  StatusBit status_bit_;
  PositionInfo position_info_ = {0};
  Jt808Info jt808_info_ = {{0}};
  UpgradeInfo upgrade_info_ = {{0}};
  AuthenticationCode authentication_code_ = {{0}};
  Message message_;
  PositionExtension custom_item_len_ = {0};
  PositionExtension position_status_ = {0};
  PositionExtension gnss_satellite_num_ = {0};
  ProtocolParameters pro_para_;
  PassThrough pass_through_;
  CanBusDataTimestamp can_bus_data_timestamp_;
  AreaRouteSet area_route_set_ = {0};
  std::vector<CanBusData> *can_bus_data_list_ = nullptr;
  std::map<uint32_t, std::string> terminal_parameter_map_;
};

#endif // JT808_TERMINAL_JT808_TERMINAL_H_
