#include "terminal/jt808_terminal.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <string>

#include "common/jt808_util.h"
#include "terminal/jt808_terminal_parameters.h"
#include "bcd/bcd.h"
#include "util/container_clear.h"


Jt808Terminal::~Jt808Terminal() {
  terminal_parameter_map_.clear();
  ClearConnect();
  ClearAreaRouteListElement(area_route_set_);
  delete [] area_route_set_.circular_area_list;
  delete [] area_route_set_.rectangle_area_list;
  delete [] area_route_set_.polygonal_area_list;
  delete [] area_route_set_.route_list;
}

int Jt808Terminal::Init() {
  message_flow_number_ = 0;
  parameter_set_type_ = 0;
  pro_para_.packet_map = nullptr;
  pro_para_.packet_id_list = nullptr;
  pro_para_.terminal_parameter_id_list = nullptr;
  if (ReadTerminalParameterFormFile(kTerminalParametersFlie,
                                    terminal_parameter_map_) < 0) {
    return -1;
  }

  alarm_bit_.value = 0;
  status_bit_.value = 0;
  status_bit_.bit.acc = 1;
  status_bit_.bit.gpsen = 1;

  gnss_satellite_num_.item_id = POSITIONEXTENSIONGNSSSATELLITENUM;
  gnss_satellite_num_.item_len = 1;
  gnss_satellite_num_.item_value[0] = 0;
  custom_item_len_.item_id = POSITIONEXTENSIONCUSTOMITEMLENGTH;
  custom_item_len_.item_len = 3;
  position_status_.item_id = POSITIONEXTENSIONPOSITIONSTATUS;
  position_status_.item_len = 1;
  position_status_.item_value[0] = 0;

  area_route_set_.circular_area_list = new std::list<CircularArea *>;
  area_route_set_.rectangle_area_list = new std::list<RectangleArea *>;
  area_route_set_.polygonal_area_list = new std::list<PolygonalArea *>;
  area_route_set_.route_list = new std::list<Route *>;
  ReadAreaRouteFormFile(kAreaRouteFlie, area_route_set_);
  WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
  memset(&position_info_, 0x0, sizeof (position_info_));
  memset(&authentication_code_, 0x0, sizeof (authentication_code_));
  return 0;
}

int Jt808Terminal::RequestConnectServer(void) {
  int socket_fd;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(jt808_info_.server_port));
  addr.sin_addr.s_addr = inet_addr(jt808_info_.server_ip);

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf("%s[%d]: create socket failed!!!\n", __FUNCTION__, __LINE__);
    return -1;
  }

  struct timeval timeout = {0, 500000};
  setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  if (connect(socket_fd, reinterpret_cast<struct sockaddr *>(&addr),
              sizeof(addr)) < 0) {
    printf("%s[%d]: connect to remote server failed!!!\n",
           __FUNCTION__, __LINE__);
    close(socket_fd);
    return -1;
  }

  socket_fd_ = socket_fd;
  return 0;

}

int Jt808Terminal::RequestLoginServer(void) {
  int retval = -1;

  if (authentication_code_.size > 0) {
    printf("%s[%d]: authenitcation code[%lu]: ", __FUNCTION__, __LINE__,
           authentication_code_.size);
    for (size_t i = 0; i < authentication_code_.size; ++i) {
      printf("%02x ", authentication_code_.buffer[i]);
    }
    printf("\n");
  } else {
    memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
    Jt808FramePack(UP_REGISTER);
    if ((retval = SendFrameData()) < 0) {
      printf("%s[%d]: register request failed!!!\n", __FUNCTION__, __LINE__);
      return -1;
    }

    while (1) {
      if ((retval = RecvFrameData()) > 0) {
        pro_para_.respond_result = kFailure;
        Jt808FrameParse();
        if (pro_para_.respond_result == kRegisterSuccess) {
          break;
        } else {
          printf("%s[%d]: register failed!!!\n", __FUNCTION__, __LINE__);
          return -1;
        }
      } else if (retval < 0) {
        return -1;
      }
    }
  }

  memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  Jt808FramePack(UP_AUTHENTICATION);
  if ((retval = SendFrameData()) < 0) {
    printf("%s[%d]: authenitcation request failed!!!\n",
           __FUNCTION__, __LINE__);
    return -1;
  }

  while (1) {
    pro_para_.respond_result = kFailure;
    if ((retval = RecvFrameData()) > 0) {
      Jt808FrameParse();
      if (pro_para_.respond_result == kSuccess) {
        is_connect_ = true;
        break;
      } else {
        memset(&authentication_code_, 0x0, sizeof(authentication_code_));
        printf("%s[%d]: authenitcation failed!!!\n", __FUNCTION__, __LINE__);
        return -1;
      }
    } else if (retval < 0) {
      return -1;
    }
  }

  return 0;
}

int Jt808Terminal::ConnectRemote(void) {
  if (RequestConnectServer() < 0) {
    printf("%s[%d]: connect server failed!!!\n", __FUNCTION__, __LINE__);
    return -1;
  }

  if (RequestLoginServer() < 0) {
    printf("%s[%d]: login server failed!!!\n", __FUNCTION__, __LINE__);
    ClearConnect();
    return -1;
  }

  return 0;
}

void Jt808Terminal::ClearConnect(void) {
  if (socket_fd_ > 0) {
    close(socket_fd_);
  }

  is_connect_ = false;
  socket_fd_ = -1;
}

int Jt808Terminal::SendFrameData(void) {
  int retval = -1;

  signal(SIGPIPE, SIG_IGN);
  retval = send(socket_fd_, message_.buffer, message_.size, 0);
  if (retval < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      retval = 0;  // Not data, but is normal.
    } else {
      if (errno == EPIPE) {
        printf("%s[%d]: remote socket close!!!\n", __FUNCTION__, __LINE__);
      }
      retval = -1;
      printf("%s[%d]: send data failed!!!\n", __FUNCTION__, __LINE__);
    }
  } else if (retval == 0) {
    printf("%s[%d]: connection disconect!!!\n", __FUNCTION__, __LINE__);
    retval = -1;
  }

  return retval;
}

int Jt808Terminal::RecvFrameData(void) {
  int retval = -1;

  memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  retval = recv(socket_fd_, message_.buffer, MAX_PROFRAMEBUF_LEN, 0);
  if (retval < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      retval = 0;  // Not data, but is normal.
    } else {
      retval = -1;
      printf("%s[%d]: recv data failed!!!\n", __FUNCTION__, __LINE__);
    }
    message_.size = 0;
  } else if(retval == 0) {
    retval = -1;
    message_.size = 0;
    printf("%s[%d]: connection disconect!!!\n", __FUNCTION__, __LINE__);
  } else {
    message_.size = retval;
  }

  return retval;
}

size_t Jt808Terminal::Jt808FramePack(const uint16_t &command) {
  MessageHead *msghead_ptr;
  PositionBasicInfo *pbi_ptr;
  RegisterInfo *reg_ptr;
  uint8_t *msg_body;
  uint8_t u8val;
  uint16_t u16val;
  uint32_t u32val;

  msghead_ptr = reinterpret_cast<MessageHead *>(&message_.buffer[1]);
  msghead_ptr->id = EndianSwap16(command);
  msghead_ptr->attribute.value = 0;
  msghead_ptr->attribute.bit.encrypt = 0;
  BcdFromStringCompress(jt808_info_.phone_number,
                        reinterpret_cast<char *>(msghead_ptr->phone),
                        strlen(jt808_info_.phone_number));
  message_flow_number_++;
  msghead_ptr->msgflownum = EndianSwap16(message_flow_number_);
  msghead_ptr->attribute.bit.package = 0;
  msghead_ptr->attribute.bit.msglen += 0;
  msg_body = &message_.buffer[MSGBODY_NOPACKAGE_POS];
  message_.size = 13;

  switch (command) {
    case UP_UNIRESPONSE:
      memcpy(&u16val, &pro_para_.respond_flow_num, 2);
      u16val = EndianSwap16(u16val);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      memcpy(&u16val, &pro_para_.respond_id, 2);
      u16val = EndianSwap16(u16val);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      *msg_body = pro_para_.respond_result;
      msg_body++;
      message_.size += 5;
      msghead_ptr->attribute.bit.msglen += 5;
      break;
    case UP_HEARTBEAT:
      msghead_ptr->attribute.bit.msglen += 0;
      break;
    case UP_REGISTER:
      reg_ptr = reinterpret_cast<RegisterInfo *>(&msg_body[0]);
      reg_ptr->provinceid = EndianSwap16(44);
      reg_ptr->cityid = EndianSwap16(300);
      memcpy(reg_ptr->manufacturerid, "SKCJS", 5);
      memcpy(reg_ptr->productmodelid, "SK9151", 6);
      memcpy(reg_ptr->productid, "1", 1);
      reg_ptr->vehicelcolor = 1;
      msg_body += 47;
      message_.size += 47;
      memset(reg_ptr->carlicense, 0x0, 10);
      msghead_ptr->attribute.bit.msglen += 47;
      break;
    case UP_LOGOUT:
      break;
    case UP_AUTHENTICATION:
      memcpy(msg_body, &authentication_code_.buffer[0],
             authentication_code_.size);
      msg_body += authentication_code_.size;
      message_.size += authentication_code_.size;
      msghead_ptr->attribute.bit.msglen += authentication_code_.size;
      break;
    case UP_GETPARARESPONSE:
      if (pro_para_.packet_total_num > 1) {
        msghead_ptr->attribute.bit.package = 1;
        u16val = EndianSwap16(pro_para_.packet_total_num);
        memcpy(&message_.buffer[13], &u16val, 2);
        u16val = EndianSwap16(pro_para_.packet_sequence_num);
        memcpy(&message_.buffer[15], &u16val, 2);
        msg_body += 4;
        message_.size += 4;
      }
      u16val = EndianSwap16(pro_para_.respond_flow_num);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      *msg_body++ = static_cast<uint8_t>(
                        pro_para_.terminal_parameter_id_list->size());
      message_.size += 3;
      msghead_ptr->attribute.bit.msglen += 3;
      for (auto &parameter_id : *pro_para_.terminal_parameter_id_list) {
        u32val = EndianSwap32(parameter_id);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        std::string parameter_value = terminal_parameter_map_[parameter_id];
        switch (GetParameterTypeByParameterId(parameter_id)) {
          case kByteType:
            *msg_body++ = 1;
            *msg_body++ = static_cast<uint8_t>(atoi(parameter_value.c_str()));
            u8val = 2;
            break;
          case kWordType:
            *msg_body++ = 2;
            u16val = static_cast<uint16_t>(atoi(parameter_value.c_str()));
            u16val = EndianSwap16(u16val);
            memcpy(msg_body, &u16val, 2);
            msg_body += 2;
            u8val = 3;
            break;
          case kDwordType:
            *msg_body++ = 4;
            u32val = static_cast<uint32_t>(atoi(parameter_value.c_str()));
            u32val = EndianSwap32(u32val);
            memcpy(msg_body, &u32val, 4);
            msg_body += 4;
            u8val = 5;
            break;
          case kStringType:
            *msg_body++ = static_cast<uint8_t>(parameter_value.size());
            memcpy(msg_body, parameter_value.c_str(), parameter_value.size());
            msg_body += parameter_value.size();
            u8val = static_cast<uint8_t>(1 + parameter_value.size());
            break;
          default:
            u8val = 0;
            break;
        }
        message_.size += 4 + u8val;
        msghead_ptr->attribute.bit.msglen += 4 + u8val;
      }
      break;
    case UP_UPGRADERESULT:
      *msg_body = pro_para_.upgrade_type;
      msg_body++;
      *msg_body = pro_para_.respond_result;
      msg_body++;
      message_.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
      break;
    case UP_VEHICLECONTROLRESPONSE:
    case UP_GETPOSITIONINFORESPONSE:
      u16val = EndianSwap16(pro_para_.respond_flow_num);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      message_.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
    case UP_POSITIONREPORT:
      pbi_ptr = reinterpret_cast<PositionBasicInfo *>(&msg_body[0]);
      pbi_ptr->alarm.value = EndianSwap32(alarm_bit_.value);
      pbi_ptr->status.value = EndianSwap32(status_bit_.value);
      u32val = static_cast<uint32_t>(position_info_.latitude * 1000000UL);
      pbi_ptr->latitude = EndianSwap32(u32val);
      u32val = static_cast<uint32_t>(position_info_.longitude * 1000000UL);
      pbi_ptr->longitude = EndianSwap32(u32val);
      u16val = static_cast<uint16_t>(position_info_.altitude);
      pbi_ptr->atitude = EndianSwap16(u16val);
      u16val = static_cast<uint16_t>(position_info_.speed/10);
      pbi_ptr->speed = EndianSwap16(u16val);
      u16val = static_cast<uint16_t>(position_info_.bearing);
      pbi_ptr->bearing = EndianSwap16(u16val);
      pbi_ptr->time[0] = BcdFromHex(position_info_.timestamp[0]);
      pbi_ptr->time[1] = BcdFromHex(position_info_.timestamp[1]);
      pbi_ptr->time[2] = BcdFromHex(position_info_.timestamp[2]);
      pbi_ptr->time[3] = BcdFromHex(position_info_.timestamp[3]);
      pbi_ptr->time[4] = BcdFromHex(position_info_.timestamp[4]);
      pbi_ptr->time[5] = BcdFromHex(position_info_.timestamp[5]);
      message_.size += sizeof(PositionBasicInfo);
      msg_body += sizeof(PositionBasicInfo);
      msghead_ptr->attribute.bit.msglen += sizeof(PositionBasicInfo);
      *msg_body++ = gnss_satellite_num_.item_id;
      *msg_body++ = gnss_satellite_num_.item_len;
      *msg_body++ = gnss_satellite_num_.item_value[0];
      message_.size += 3;
      msghead_ptr->attribute.bit.msglen += 3;
      if (custom_item_len_.item_len == 3) {
        *msg_body++ = custom_item_len_.item_id;
        *msg_body++ = custom_item_len_.item_len;
        *msg_body++ = position_status_.item_id;
        *msg_body++ = position_status_.item_len;
        *msg_body++ = position_status_.item_value[0];
        message_.size += 5;
        msghead_ptr->attribute.bit.msglen += 5;
      }
      break;
    case UP_CANBUSDATAUPLOAD:
      *msg_body++ = static_cast<uint8_t>(can_bus_data_list_->size());
      *msg_body++ = BcdFromHex(can_bus_data_timestamp_.hour);
      *msg_body++ = BcdFromHex(can_bus_data_timestamp_.minute);
      *msg_body++ = BcdFromHex(can_bus_data_timestamp_.second);
      *msg_body++ = BcdFromHex(can_bus_data_timestamp_.millisecond/100);
      *msg_body++ = BcdFromHex(can_bus_data_timestamp_.millisecond%100);
      message_.size += 5;
      msghead_ptr->attribute.bit.msglen += 5;
      if (!can_bus_data_list_->empty()) {
        for (auto &can_bus_data : *can_bus_data_list_) {
          memcpy(msg_body, &can_bus_data.can_id.value, 4);
          msg_body += 4;
          memcpy(msg_body, can_bus_data.buffer, 8);
          msg_body += 8;
          message_.size += 12;
          msghead_ptr->attribute.bit.msglen += 12;
        }
      }
      break;
    case UP_PASSTHROUGH:
      *msg_body = pass_through_.type;
      msg_body++;
      message_.size++;
      memcpy(msg_body, pass_through_.buffer, pass_through_.size);
      message_.size += pass_through_.size;
      msghead_ptr->attribute.bit.msglen += pass_through_.size;
      break;
    case DOWN_PACKETRESEND:
      u16val = EndianSwap16(pro_para_.packet_first_flow_num);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      *msg_body = static_cast<uint8_t>(pro_para_.packet_id_list->size());
      msg_body++;
      for (auto &packet_id : *pro_para_.packet_id_list) {
        u16val = EndianSwap16(packet_id);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
      }
      message_.size += 3 + pro_para_.packet_id_list->size();
      msghead_ptr->attribute.bit.msglen += 3 + pro_para_.packet_id_list->size();
      break;
    default:
      break;
  }
  u16val = msghead_ptr->attribute.value;
  msghead_ptr->attribute.value = EndianSwap16(u16val);

  *msg_body = BccCheckSum(&message_.buffer[1], message_.size - 1);
  message_.size++;
  message_.size = Escape(&message_.buffer[1], message_.size);
  message_.buffer[0] = PROTOCOL_SIGN;
  message_.buffer[message_.size++] = PROTOCOL_SIGN;

  printf("%s[%d]: socket-send[%lu]:\n", __FUNCTION__, __LINE__, message_.size);
  for (uint16_t i = 0; i < message_.size; ++i) {
    printf("%02X ", message_.buffer[i]);
  }
  printf("\r\n");

  return message_.size;
}

uint16_t Jt808Terminal::Jt808FrameParse() {
  uint8_t *msg_body;
  uint8_t count;
  uint8_t operation;
  uint8_t u8val;
  uint16_t u16val;
  uint16_t message_id;
  uint32_t u32val;
  std::ofstream ofs;
  std::vector<uint32_t> terminal_parameter_id_list;
  MessageHead *msghead_ptr;
  MessageBodyAttr msgbody_attribute;

  // printf("%s[%d]: socket-recv[%lu]:\n",
  //        __FUNCTION__, __LINE__, message_.size);
  // for (size_t i = 0; i < message_.size; ++i)
  //   printf("%02X ", message_.buffer[i]);
  // printf("\n");

  message_.size = ReverseEscape(&message_.buffer[1],
                                    message_.size);
  msghead_ptr = reinterpret_cast<MessageHead *>(&message_.buffer[1]);
  memcpy(&msgbody_attribute.value, &msghead_ptr->attribute.value, 2);
  msgbody_attribute.value = EndianSwap16(msgbody_attribute.value);
  if (msgbody_attribute.bit.package) {
    msg_body = &message_.buffer[MSGBODY_PACKAGE_POS];
  } else {
    msg_body = &message_.buffer[MSGBODY_NOPACKAGE_POS];
  }

  memcpy(&u16val, &msghead_ptr->msgflownum, 2);
  pro_para_.respond_flow_num = EndianSwap16(u16val);
  memcpy(&u16val, &msghead_ptr->id, 2);
  message_id = EndianSwap16(u16val);
  pro_para_.respond_id = message_id;
  switch (message_id) {
    case DOWN_UNIRESPONSE:
      memcpy(&u16val, &msg_body[2], 2);
      pro_para_.respond_id = EndianSwap16(u16val);
      switch(pro_para_.respond_id) {
        case UP_HEARTBEAT:
          printf("%s[%d]: received heartbeat respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case UP_AUTHENTICATION:
          printf("%s[%d]: received authenitcation respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case UP_GETPARARESPONSE:
          printf("%s[%d]: received get terminal parameter response respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case UP_POSITIONREPORT:
          printf("%s[%d]: received position report respond: ",
                  __FUNCTION__, __LINE__);
          break;
        case UP_GETPOSITIONINFORESPONSE:
          printf("%s[%d]: received get position info response respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case UP_VEHICLECONTROLRESPONSE:
          printf("%s[%d]: received vehicle control response respond: ",
                  __FUNCTION__, __LINE__);
          break;
        case UP_PASSTHROUGH:
          printf("%s[%d]: received up passthrough respond: ",
                  __FUNCTION__, __LINE__);
          break;
        case DOWN_PACKETRESEND:
          printf("%s[%d]: received packet resend respond: ",
                  __FUNCTION__, __LINE__);
          break;
        default:
          printf("%s[%d]: received undefined respond: ",
                  __FUNCTION__, __LINE__);
          break;
      }
      if (msg_body[4] == 0x00) {
        pro_para_.respond_result = kSuccess;
        printf("normal\r\n");
      } else if(msg_body[4] == 0x01) {
        pro_para_.respond_result = kFailure;
        printf("failed\r\n");
      } else if(msg_body[4] == 0x02) {
        pro_para_.respond_result = kMessageHasWrong;
        printf("message has something wrong\r\n");
      } else if(msg_body[4] == 0x03) {
        pro_para_.respond_result = kNotSupport;
        printf("message not support\r\n");
      }
      break;
    case DOWN_REGISTERRESPONSE:
      printf("%s[%d]: received register response: ", __FUNCTION__, __LINE__);
      authentication_code_.size = msgbody_attribute.bit.msglen - 3;
      if (msg_body[2] == 0x00) {
        pro_para_.respond_result = kRegisterSuccess;
        printf("normal\r\n");
        memcpy(authentication_code_.buffer, msg_body + 3,
               authentication_code_.size);
        printf("%s[%d]: authenitcation code:", __FUNCTION__, __LINE__);
        for (size_t i = 0; i < authentication_code_.size; ++i) {
          printf(" %02X", authentication_code_.buffer[i]);
        }
        printf("\n");
      } else if(msg_body[2] == 0x01) {
        pro_para_.respond_result = kVehiclesHaveBeenRegistered;
        printf("vehicle has been registered\r\n");
      } else if(msg_body[2] == 0x02) {
        pro_para_.respond_result = kNoSuchVehicleInTheDatabase;
        printf("no such vehicle in the database\r\n");
      } else if(msg_body[2] == 0x03) {
        pro_para_.respond_result = kTerminalHaveBeenRegistered;
        printf("terminal has been registered\r\n");
      } else if(msg_body[2] == 0x04) {
        pro_para_.respond_result = kNoSuchTerminalInTheDatabase;
        printf("no such terminal in the database\r\n");
      }
      break;
    case DOWN_SETTERMPARA:
      int para_count;
      char value[256];
      para_count = static_cast<int>(msg_body[0]);
      msg_body++;
      while (para_count-- > 0) {
        if (terminal_parameter_map_.empty()) {
          pro_para_.respond_result = kFailure;
          break;
        }
        memcpy(&u32val, msg_body, 4);  // terminal parameter id.
        u32val = EndianSwap32(u32val);
        msg_body += 4;
        u8val = msg_body[0];  // terminal parameter len.
        msg_body++;
        for (auto &parameter : terminal_parameter_map_) {
          if (parameter.first == u32val) {
            switch (GetParameterTypeByParameterId(u32val)) {
              case kByteType:
                u8val = *msg_body;
                parameter.second = std::to_string(u8val);
                break;
              case kWordType:
                memcpy(&u16val, &msg_body[0], 2);
                u16val = EndianSwap16(u16val);
                parameter.second = std::to_string(u16val);
                break;
              case kDwordType:
                memcpy(&u32val, &msg_body[0], 4);
                u32val = EndianSwap32(u32val);
                parameter.second = std::to_string(u32val);
                break;
              case kStringType:
                memset(value, 0x0, 256);
                memcpy(value, &msg_body[0], u8val);
                parameter.second = value;
                break;
              default:
                printf("unknow type\n");
                break;
            }
            break;
          }
        }
        msg_body += u8val;
      }

      if (para_count < 0) {
        pro_para_.respond_result = kSuccess;
        WriteTerminalParameterToFile(kTerminalParametersFlie,
                                     terminal_parameter_map_);
      }
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_GETTERMPARA:
    case DOWN_GETSPECTERMPARA:
      uint16_t data_len;
      uint8_t parameter_type;
      uint16_t respond_flow_num;
      if (message_id == DOWN_GETTERMPARA) {  // Get all terminal parameter.
        pro_para_.respond_para_num = static_cast<uint8_t>(
                                         terminal_parameter_map_.size());
        for (auto &parameter : terminal_parameter_map_) {
          terminal_parameter_id_list.push_back(parameter.first);
        }
      } else {
        pro_para_.respond_para_num = msg_body[0];
        if (PrepareTerminalParameterIdList(&msg_body[1], msg_body[0],
                                           terminal_parameter_map_,
                                           &terminal_parameter_id_list) < 0) {
          memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
          pro_para_.respond_result = kNotSupport;
          Jt808FramePack(UP_UNIRESPONSE);
          SendFrameData();
          return message_id;
        }
      }
      data_len = 0;
      for (auto &parameter_id : terminal_parameter_id_list) {
        data_len += 5;
        parameter_type = GetParameterTypeByParameterId(parameter_id);
        if (parameter_type != kStringType) {
          data_len += GetParameterLengthByParameterType(parameter_type);
        } else {
          data_len += terminal_parameter_map_[parameter_id].size();
        }
      }
      if (data_len > MAX_TERMINAL_PARAMETER_LEN_A_RECORD) {  // Need packet.
        respond_flow_num = pro_para_.respond_flow_num;
        pro_para_.packet_total_num =
            data_len/MAX_TERMINAL_PARAMETER_LEN_A_RECORD + 1;
        pro_para_.packet_sequence_num = 1;
      }
      int retval;
      if (pro_para_.terminal_parameter_id_list == nullptr) {
        pro_para_.terminal_parameter_id_list = new std::vector<uint32_t>;
      }
      for (auto parameter_id_it = terminal_parameter_id_list.begin();
           parameter_id_it != terminal_parameter_id_list.end(); ) {
        data_len = 0;
        while (parameter_id_it != terminal_parameter_id_list.end()) {
          data_len += 5;
          parameter_type = GetParameterTypeByParameterId(*parameter_id_it);
          if (parameter_type != kStringType) {
            data_len += GetParameterLengthByParameterType(parameter_type);
          } else {
            data_len += terminal_parameter_map_[*parameter_id_it].size();
          }
          if (data_len > MAX_TERMINAL_PARAMETER_LEN_A_RECORD) break;
          pro_para_.terminal_parameter_id_list->push_back(*parameter_id_it);
          ++parameter_id_it;
        }
        memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        if (pro_para_.packet_total_num > 0) {
          pro_para_.respond_flow_num = respond_flow_num;
        }
        Jt808FramePack(UP_GETPARARESPONSE);
        pro_para_.terminal_parameter_id_list->clear();
        SendFrameData();
        while (1) {
          if ((retval = RecvFrameData()) > 0) {
            Jt808FrameParse();
            if (pro_para_.respond_id == UP_GETPARARESPONSE) {
              break;
            }
          } else if (retval < 0) {
            break;
          }
        }
        if (retval < 0) break;
        if (pro_para_.packet_total_num > pro_para_.packet_sequence_num) {
          ++pro_para_.packet_sequence_num;
        }
      }
      delete pro_para_.terminal_parameter_id_list;
      pro_para_.terminal_parameter_id_list = nullptr;
      terminal_parameter_id_list.clear();
      pro_para_.packet_total_num = pro_para_.packet_sequence_num = 0;
      break;
    case DOWN_TERMINALCONTROL:
      terminal_control_type_ = *msg_body++;
      pro_para_.respond_result = 0;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_UPGRADEPACKAGE:
      uint8_t *packet_ptr;
      uint16_t packet_seq;
      uint16_t packet_total;
      uint32_t packet_len;
      printf("%s[%d]: received upgrade package\r\n", __FUNCTION__, __LINE__);
      if (msgbody_attribute.bit.package) {
        memcpy(&u16val, &message_.buffer[13], 2);
        pro_para_.packet_total_num = EndianSwap16(u16val);
        memcpy(&u16val, &message_.buffer[15], 2);
        packet_seq = EndianSwap16(u16val);
        if (pro_para_.packet_map == nullptr) {
          pro_para_.packet_map = new std::map<uint16_t, Message>;
        }
        if (pro_para_.packet_id_list == nullptr) {
          pro_para_.packet_id_list = new std::list<uint16_t>;
          pro_para_.packet_first_flow_num = pro_para_.respond_flow_num;
        }
      } else {
        pro_para_.packet_total_num = packet_seq = 0;
      }
      if (message_.buffer[message_.size-2] !=
          BccCheckSum(&message_.buffer[1], message_.size-3)) {
        pro_para_.respond_result = kMessageHasWrong;
        printf("%s[%d]: check sum error, %02X, %02X\n", __FUNCTION__, __LINE__,
               message_.buffer[message_.size-2],
               BccCheckSum(&message_.buffer[1], message_.size-3));
        pro_para_.packet_id_list->push_back(packet_seq);
        memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        Jt808FramePack(UP_UNIRESPONSE);
        SendFrameData();
        break;
      } else {
        if (pro_para_.packet_total_num) {
          pro_para_.packet_map->insert(std::make_pair(packet_seq, message_));
        } else {
          pro_para_.packet_map->insert(std::make_pair(1, message_));
        }
        pro_para_.respond_result = kSuccess;
        memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        Jt808FramePack(UP_UNIRESPONSE);
        SendFrameData();
      }
      if (!pro_para_.packet_id_list->empty()) {
        packet_total = pro_para_.packet_id_list->back();
        for (auto packet_id_it = pro_para_.packet_id_list->begin();
             packet_id_it != pro_para_.packet_id_list->end(); ) {
          if (*packet_id_it == packet_seq) {
            pro_para_.packet_id_list->erase(packet_id_it);
            break;
          } else {
            ++packet_id_it;
          }
        }
      } else {
        packet_total = pro_para_.packet_total_num;
      }
      if ((packet_total == packet_seq) &&
          (pro_para_.packet_map->size() != pro_para_.packet_total_num)) {
        if (pro_para_.packet_id_list->empty()) {
          packet_seq = 1;
          auto packet_msg_it = pro_para_.packet_map->begin();
          for ( ; packet_seq <= pro_para_.packet_total_num; ++packet_seq) {
            if ((packet_msg_it != pro_para_.packet_map->end()) &&
                (packet_seq == packet_msg_it->first)) {
              packet_msg_it++;
              continue;
            }
            pro_para_.packet_id_list->push_back(packet_seq);
          }
        }
        memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        Jt808FramePack(DOWN_PACKETRESEND);
        SendFrameData();
      } else if ((pro_para_.packet_total_num == 0) ||
                 (pro_para_.packet_map->size() == pro_para_.packet_total_num)) {
        message_ = pro_para_.packet_map->begin()->second;
        if (pro_para_.packet_total_num) {
          msg_body = &message_.buffer[MSGBODY_PACKAGE_POS];
        } else {
          msg_body = &message_.buffer[MSGBODY_NOPACKAGE_POS];
        }
        memset(&upgrade_info_, 0x0, sizeof(upgrade_info_));
        memcpy(upgrade_info_.version_id, &msg_body[7], msg_body[6]);
        uint8_t upgrade_type_id = msg_body[0];
        switch (upgrade_type_id) {
          case 0x0:  // 终端.
            upgrade_info_.upgrade_type = kDeviceUpgrade;
            snprintf(upgrade_info_.file_path, sizeof(upgrade_info_.file_path),
                     "%s/%s-Ver%s.bin", kDownloadDir, "DEVICE",
                     msg_body[6] > 0 ? upgrade_info_.version_id : "1.0.0");
            break;
          case 0xc:  // 道路运输证IC卡读卡器.
            snprintf(upgrade_info_.file_path, sizeof(upgrade_info_.file_path),
                     "%s/%s-Ver%s.bin", kDownloadDir, "ICCARD",
                     msg_body[6] > 0 ? upgrade_info_.version_id : "1.0.0");
            break;
          case 0x34:  // 北斗卫星定位模块
            upgrade_info_.upgrade_type = kGpsUpgrade;
            snprintf(upgrade_info_.file_path, sizeof(upgrade_info_.file_path),
                     "%s/%s-Ver%s.bin", kDownloadDir, "GPS",
                     msg_body[6] > 0 ? upgrade_info_.version_id : "20180922");
            break;
          default: // Unknow type id, return not support msessage.
            pro_para_.respond_result = kNotSupport;
            memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
            Jt808FramePack(UP_UNIRESPONSE);
            SendFrameData();
            return message_id;
        }
        // In case it already exists.
        unlink(upgrade_info_.file_path);
        // Write to file.
        ofs.open(upgrade_info_.file_path,
                 std::ios::binary | std::ios::out | std::ios::app);
        if (ofs.is_open()) {
          if (pro_para_.packet_total_num == 0) {
            memcpy(&u32val, &msg_body[7+msg_body[6]], 4);
            packet_len = EndianSwap32(u32val);
            packet_ptr = &msg_body[11+msg_body[6]];
            ofs.write(reinterpret_cast<char *>(packet_ptr), packet_len);
          } else {
            for (auto &element : *pro_para_.packet_map) {
               msg_body = &element.second.buffer[MSGBODY_PACKAGE_POS];
               memcpy(&u32val, &msg_body[7+msg_body[6]], 4);
               packet_len = EndianSwap32(u32val);
               packet_ptr = &msg_body[11+msg_body[6]];
               ofs.write(reinterpret_cast<char *>(packet_ptr), packet_len);
            }
            pro_para_.packet_map->erase(pro_para_.packet_map->begin(),
                                        pro_para_.packet_map->end());
            if (!pro_para_.packet_id_list->empty()) {
              pro_para_.packet_id_list->clear();
            }
            delete pro_para_.packet_map;
            delete pro_para_.packet_id_list;
            pro_para_.packet_map = nullptr;
            pro_para_.packet_id_list = nullptr;
          }
          ofs.close();
          pro_para_.packet_total_num = 0;
        }
        std::string str = "echo 0 > \"/tmp/JT808UPG&&";
        if (upgrade_info_.upgrade_type == kGpsUpgrade) {
          str += "GPS&&";
        } else if (upgrade_info_.upgrade_type == kDeviceUpgrade) {
          str += "DEVICE&&";
        } else {
          break;
        }
        str += upgrade_info_.version_id;
        str += "&&false\"";
        system(str.c_str());
      }
      break;
    case DOWN_GETPOSITIONINFO:
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_GETPOSITIONINFORESPONSE);
      SendFrameData();
      break;
    case DOWN_POSITIONTRACK:
      memcpy(&u16val, &msg_body[0], 2);
      u16val = EndianSwap16(u16val);
      report_interval_ = static_cast<int>(u16val);
      msg_body += 2;
      memcpy(&u32val, &msg_body[0], 4);
      u32val = EndianSwap32(u32val);
      report_valid_time_ = static_cast<int64_t>(u32val);
      msg_body += 4;
      pro_para_.respond_result = 0;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_VEHICLECONTROL:
      pro_para_.vehicle_control_flag.value = *msg_body++;
      status_bit_.bit.doorlock = pro_para_.vehicle_control_flag.bit.doorlock;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_VEHICLECONTROLRESPONSE);
      SendFrameData();
      break;
    case DOWN_SETCIRCULARAREA:
      operation = *msg_body++;
      count = *msg_body++;
      for (int i = 0; i < count; ++i) {
        CircularArea *circular_area = new CircularArea;
        memcpy(&u32val, msg_body, 4);
        circular_area->area_id = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u16val, msg_body, 2);
        circular_area->area_attribute.value = EndianSwap16(u16val);
        msg_body += 2;
        memcpy(&u32val, msg_body, 4);
        circular_area->center_point.latitude = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u32val, msg_body, 4);
        circular_area->center_point.longitude = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u32val, msg_body, 4);
        circular_area->radius = EndianSwap32(u32val);
        msg_body += 4;
        if (circular_area->area_attribute.bit.bytime) {
          memcpy(circular_area->start_time, msg_body, 6);
          msg_body += 6;
          memcpy(circular_area->end_time, msg_body, 6);
          msg_body += 6;
        }
        if (circular_area->area_attribute.bit.speedlimit) {
          memcpy(&u16val, msg_body, 2);
          circular_area->max_speed = EndianSwap16(u16val);
          msg_body += 2;
          circular_area->overspeed_duration = *msg_body++;
        }
        if (area_route_set_.circular_area_list == nullptr) {
          area_route_set_.circular_area_list = new std::list<CircularArea *>;
        }
        if (operation == kAppendArea) {
          area_route_set_.circular_area_list->push_back(circular_area);
        } else {
          auto area_it = area_route_set_.circular_area_list->begin();
          while (area_it != area_route_set_.circular_area_list->end()) {
            if ((*area_it)->area_id == circular_area->area_id) {
              delete *area_it;
              area_route_set_.circular_area_list->erase(area_it);
              area_route_set_.circular_area_list->push_back(circular_area);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (count > 0) {
        WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_DELCIRCULARAREA:
      count = *msg_body++;
      if (count == 0) {
        ClearContainerElement(area_route_set_.circular_area_list);
        pro_para_.respond_result = kSuccess;
      } else if (area_route_set_.circular_area_list != nullptr) {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, msg_body, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set_.circular_area_list->begin();
          while (area_it != area_route_set_.circular_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              delete *area_it;
              area_it = area_route_set_.circular_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_SETRECTANGLEAREA:
      operation = *msg_body++;
      count = *msg_body++;
      for (int i = 0; i < count; ++i) {
        RectangleArea *rectangle_area = new RectangleArea;
        memcpy(&u32val, msg_body, 4);
        rectangle_area->area_id = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u16val, msg_body, 2);
        rectangle_area->area_attribute.value = EndianSwap16(u16val);
        msg_body += 2;
        memcpy(&u32val, msg_body, 4);
        rectangle_area->upper_left_corner.latitude = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u32val, msg_body, 4);
        rectangle_area->upper_left_corner.longitude = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u32val, msg_body, 4);
        rectangle_area->bottom_right_corner.latitude = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u32val, msg_body, 4);
        rectangle_area->bottom_right_corner.longitude = EndianSwap32(u32val);
        msg_body += 4;
        if (rectangle_area->area_attribute.bit.bytime) {
          memcpy(rectangle_area->start_time, msg_body, 6);
          msg_body += 6;
          memcpy(rectangle_area->end_time, msg_body, 6);
          msg_body += 6;
        }
        if (rectangle_area->area_attribute.bit.speedlimit) {
          memcpy(&u16val, msg_body, 2);
          rectangle_area->max_speed = EndianSwap16(u16val);
          msg_body += 2;
          rectangle_area->overspeed_duration = *msg_body++;
        }
        if (area_route_set_.rectangle_area_list == nullptr) {
          area_route_set_.rectangle_area_list = new std::list<RectangleArea *>;
        }
        if (operation == kAppendArea) {
          area_route_set_.rectangle_area_list->push_back(rectangle_area);
        } else {
          auto area_it = area_route_set_.rectangle_area_list->begin();
          while (area_it != area_route_set_.rectangle_area_list->end()) {
            if ((*area_it)->area_id == rectangle_area->area_id) {
              delete *area_it;
              area_route_set_.rectangle_area_list->erase(area_it);
              area_route_set_.rectangle_area_list->push_back(rectangle_area);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (count > 0) {
        WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_DELRECTANGLEAREA:
      count = *msg_body++;
      if (count == 0) {
        ClearContainerElement(area_route_set_.rectangle_area_list);
        pro_para_.respond_result = kSuccess;
      } else if (area_route_set_.rectangle_area_list != nullptr) {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, msg_body, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set_.rectangle_area_list->begin();
          while (area_it != area_route_set_.rectangle_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              delete *area_it;
              area_route_set_.rectangle_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_SETPOLYGONALAREA:
      operation = *msg_body++;
      count = *msg_body++;
      for (int i = 0; i < count; ++i) {
        PolygonalArea *polygonal_area = new PolygonalArea;
        memcpy(&u32val, msg_body, 4);
        polygonal_area->area_id = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u16val, msg_body, 2);
        polygonal_area->area_attribute.value = EndianSwap16(u16val);
        msg_body += 2;
        if (polygonal_area->area_attribute.bit.bytime) {
          memcpy(polygonal_area->start_time, msg_body, 6);
          msg_body += 6;
          memcpy(polygonal_area->end_time, msg_body, 6);
          msg_body += 6;
        }
        if (polygonal_area->area_attribute.bit.speedlimit) {
          memcpy(&u16val, msg_body, 2);
          polygonal_area->max_speed = EndianSwap16(u16val);
          msg_body += 2;
          polygonal_area->overspeed_duration = *msg_body++;
        }
        memcpy(&u16val, msg_body, 2);
        polygonal_area->coordinate_count = EndianSwap16(u16val);
        msg_body += 2;
        polygonal_area->coordinate_list = new std::vector<Coordinate*>;
        for (int i = 0; i < polygonal_area->coordinate_count; ++i) {
          Coordinate *coordinate = new Coordinate;
          memcpy(&u32val, msg_body, 4);
          coordinate->latitude = EndianSwap32(u32val);
          msg_body += 4;
          memcpy(&u32val, msg_body, 4);
          coordinate->longitude = EndianSwap32(u32val);
          msg_body += 4;
          polygonal_area->coordinate_list->push_back(coordinate);
        }
        if (area_route_set_.polygonal_area_list == nullptr) {
          area_route_set_.polygonal_area_list = new std::list<PolygonalArea *>;
        }
        if (operation == kAppendArea) {
          area_route_set_.polygonal_area_list->push_back(polygonal_area);
        } else {
          auto area_it = area_route_set_.polygonal_area_list->begin();
          while (area_it != area_route_set_.polygonal_area_list->end()) {
            if ((*area_it)->area_id == polygonal_area->area_id) {
              ClearContainerElement((*area_it)->coordinate_list);
              delete (*area_it)->coordinate_list;
              delete *area_it;
              area_route_set_.polygonal_area_list->erase(area_it);
              area_route_set_.polygonal_area_list->push_back(polygonal_area);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (count > 0) {
        WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_DELPOLYGONALAREA:
      count = *msg_body++;
      if (count == 0) {
        if (area_route_set_.polygonal_area_list != nullptr) {
          auto area_it = area_route_set_.polygonal_area_list->begin();
          while (area_it != area_route_set_.polygonal_area_list->end()) {
            ClearContainerElement((*area_it)->coordinate_list);
            delete (*area_it)->coordinate_list;
            ++area_it;
          }
        }
        ClearContainerElement(area_route_set_.polygonal_area_list);
        pro_para_.respond_result = kSuccess;
      } else if (area_route_set_.polygonal_area_list != nullptr) {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, msg_body, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set_.polygonal_area_list->begin();
          while (area_it != area_route_set_.polygonal_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              ClearContainerElement((*area_it)->coordinate_list);
              delete (*area_it)->coordinate_list;
              delete *area_it;
              area_route_set_.polygonal_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
      pro_para_.respond_result = kSuccess;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_SETROUTE:
      operation = *msg_body++;
      count = *msg_body++;
      for (int i = 0; i < count; ++i) {
        Route *route = new Route;
        memcpy(&u32val, msg_body, 4);
        route->route_id = EndianSwap32(u32val);
        msg_body += 4;
        memcpy(&u16val, msg_body, 2);
        route->route_attribute.value = EndianSwap16(u16val);
        msg_body += 2;
        if (route->route_attribute.bit.bytime) {
          memcpy(route->start_time, msg_body, 6);
          msg_body += 6;
          memcpy(route->end_time, msg_body, 6);
          msg_body += 6;
        }
        memcpy(&u16val, msg_body, 2);
        route->inflection_point_count = EndianSwap16(u16val);
        msg_body += 2;
        route->inflection_point_list = new std::vector<InflectionPoint *>;
        for (int i = 0; i < route->inflection_point_count; ++i) {
          InflectionPoint *inflection_point = new InflectionPoint;
          memcpy(&u32val, msg_body, 4);
          inflection_point->inflection_point_id = EndianSwap32(u32val);
          msg_body += 4;
          memcpy(&u32val, msg_body, 4);
          inflection_point->road_section_id = EndianSwap32(u32val);
          msg_body += 4;
          memcpy(&u32val, msg_body, 4);
          inflection_point->coordinate.latitude = EndianSwap32(u32val);
          msg_body += 4;
          memcpy(&u32val, msg_body, 4);
          inflection_point->coordinate.longitude = EndianSwap32(u32val);
          msg_body += 4;
          inflection_point->road_section_wide = *msg_body++;
          inflection_point->road_section_attribute.value = *msg_body++;
          if (inflection_point->road_section_attribute.bit.traveltime) {
            memcpy(&u16val, msg_body, 2);
            inflection_point->max_driving_time = EndianSwap16(u16val);
            msg_body += 2;
            memcpy(&u16val, msg_body, 2);
            inflection_point->min_driving_time = EndianSwap16(u16val);
            msg_body += 2;
          }
          if (inflection_point->road_section_attribute.bit.speedlimit) {
            memcpy(&u16val, msg_body, 2);
            inflection_point->max_speed = EndianSwap16(u16val);
            msg_body += 2;
            inflection_point->overspeed_duration = *msg_body++;
          }
          route->inflection_point_list->push_back(inflection_point);
        }
        if (area_route_set_.route_list == nullptr) {
          area_route_set_.route_list = new std::list<Route *>;
        }
        if (operation == kAppendArea) {
          area_route_set_.route_list->push_back(route);
        } else {
          auto route_it = area_route_set_.route_list->begin();
          while (route_it != area_route_set_.route_list->end()) {
            if ((*route_it)->route_id == route->route_id) {
              ClearContainerElement((*route_it)->inflection_point_list);
              delete (*route_it)->inflection_point_list;
              delete *route_it;
              area_route_set_.route_list->erase(route_it);
              area_route_set_.route_list->push_back(route);
              break;
            } else {
              ++route_it;
            }
          }
        }
      }
      if (count > 0) {
        WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_DELROUTE:
      count = *msg_body++;
      if (count == 0) {
        if (area_route_set_.route_list != nullptr) {
          auto route_it = area_route_set_.route_list->begin();
          while (route_it != area_route_set_.route_list->end()) {
            ClearContainerElement((*route_it)->inflection_point_list);
            delete (*route_it)->inflection_point_list;
            ++route_it;
          }
        }
        ClearContainerElement(area_route_set_.route_list);
        pro_para_.respond_result = kSuccess;
      } else if (area_route_set_.route_list != nullptr) {
        uint32_t route_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, msg_body, 4);
          route_id = EndianSwap32(u32val);
          auto route_it = area_route_set_.route_list->begin();
          while (route_it != area_route_set_.route_list->end()) {
            if ((*route_it)->route_id == route_id) {
              ClearContainerElement((*route_it)->inflection_point_list);
              delete (*route_it)->inflection_point_list;
              delete *route_it;
              area_route_set_.route_list->erase(route_it);
              break;
            } else {
              ++route_it;
            }
          }
        }
        pro_para_.respond_result = kSuccess;
      } else {
        pro_para_.respond_result = kFailure;
      }
      WriteAreaRouteToFile(kAreaRouteFlie, area_route_set_);
      pro_para_.respond_result = kSuccess;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    case DOWN_PASSTHROUGH:
      printf("%s[%d]: received down passthrough\r\n", __FUNCTION__, __LINE__);
      pass_through_.type = *msg_body;
      msg_body++;
      pass_through_.size = msgbody_attribute.bit.msglen - 1;
      memcpy(pass_through_.buffer, msg_body, pass_through_.size);
      pro_para_.respond_result = kSuccess;
      memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      Jt808FramePack(UP_UNIRESPONSE);
      SendFrameData();
      break;
    default:
      break;
  }
  return message_id;
}

int Jt808Terminal::ReportPosition(void) {
  memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  Jt808FramePack(UP_POSITIONREPORT);
  return (SendFrameData() >= 0 ? 0 : -1);
}

int Jt808Terminal::HeartBeat(void) {
  memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  Jt808FramePack(UP_HEARTBEAT);
  return (SendFrameData() >= 0 ? 0 : -1);
}

void Jt808Terminal::ReportUpgradeResult(void) {
  struct dirent *dirent_ptr;
  std::string str;
  DIR *dir_ptr = opendir("/tmp");

  while ((dirent_ptr = readdir(dir_ptr)) != nullptr) {
    if (dirent_ptr->d_type == DT_REG) {
      str = dirent_ptr->d_name;
      if (str.find("JT808UPG&&") != std::string::npos) {
        memset(&pro_para_, 0x0, sizeof(pro_para_));
        if (str.find("GPS") != std::string::npos) {
          pro_para_.upgrade_type = 0x34;
        } else if (str.find("DEVICE") != std::string::npos) {
          pro_para_.upgrade_type = 0x0;
        }
        if (str.find("true") != std::string::npos) {
          pro_para_.respond_result = kSuccess;
        } else if (str.find("false") != std::string::npos) {
          pro_para_.respond_result = kFailure;
        } else {
          pro_para_.respond_result = kNotSupport;
        }
        memset(message_.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        Jt808FramePack(UP_UPGRADERESULT);
        if (!SendFrameData()) {
          str = "/tmp/" + str;
          // Remove file.
          unlink(str.c_str());
        }
      }
    }
  }

  closedir(dir_ptr);
  dir_ptr = nullptr;
}

