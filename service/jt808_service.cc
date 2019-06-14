#include "service/jt808_service.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>

#include "util/container_clear.h"


Jt808Service::~Jt808Service() {
  if (listen_sock_ > 0) {
    close(listen_sock_);
  }
  if (epoll_fd_ > 0) {
    close(epoll_fd_);
  }
  device_list_.clear();
  delete [] epoll_events_;
}

bool Jt808Service::Init(const int &port, const int &max_count) {
  max_count_ = max_count;
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_ == -1) {
    exit(1);
  }

  if (bind(listen_sock_, reinterpret_cast<struct sockaddr*>(&server_addr),
           sizeof(struct sockaddr)) == -1) {
    exit(1);
  }

  if (listen(listen_sock_, 5) == -1) {
    exit(1);
  }

  epoll_events_ = new struct epoll_event[max_count_];
  if (epoll_events_ == NULL) {
    exit(1);
  }

  epoll_fd_ = epoll_create(max_count_);
  EpollRegister(epoll_fd_, listen_sock_);

  ReadDevicesList(kDevicesFilePath, device_list_);

  socket_fd_ = ServerListen(kCommandInterfacePath);
  EpollRegister(epoll_fd_, socket_fd_);

  return true;
}

bool Jt808Service::Init(const char *ip, const int &port, const int &max_count) {
  max_count_ = max_count;
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  server_addr.sin_addr.s_addr = inet_addr(ip);

  listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_ == -1) {
    exit(1);
  }

  if (bind(listen_sock_, reinterpret_cast<struct sockaddr*>(&server_addr),
           sizeof(struct sockaddr)) == -1) {
    exit(1);
  }

  if (listen(listen_sock_, 5) == -1) {
    exit(1);
  }

  epoll_events_ = new struct epoll_event[max_count_];
  if (epoll_events_ == NULL) {
    exit(1);
  }

  epoll_fd_ = epoll_create(max_count_);
  EpollRegister(epoll_fd_, listen_sock_);

  ReadDevicesList(kDevicesFilePath, device_list_);

  socket_fd_ = ServerListen(kCommandInterfacePath);
  EpollRegister(epoll_fd_, socket_fd_);

  return true;
}

int Jt808Service::AcceptNewClient(void) {
  uint16_t command = 0;
  struct sockaddr_in client_addr;
  char phone_num[6] = {0};
  decltype (device_list_.begin()) device_it;
  ProtocolParameters propara = {0};
  Message msg = {0};

  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t clilen = sizeof(struct sockaddr);
  int new_sock = accept(listen_sock_,
                        reinterpret_cast<struct sockaddr*>(&client_addr),
                        &clilen);

  int keepalive = 1;  // enable keepalive attributes.
  int keepidle = 30;  // time out for starting detection.
  int keepinterval = 5;  // time interval for sending packets during detection.
  int keepcount = 3;  // max times for sending packets during detection.
  setsockopt(new_sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPINTVL,
             &keepinterval, sizeof(keepinterval));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));

  if (!RecvFrameData(new_sock, msg)) {
    memset(&propara, 0x0, sizeof(propara));
    command = Jt808FrameParse(msg, propara);
    switch (command) {
      case UP_REGISTER:
        memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        msg.size = Jt808FramePack(msg, DOWN_REGISTERRESPONSE, propara);
        SendFrameData(new_sock, msg);
        if (propara.respond_result != kSuccess) {
          close(new_sock);
          new_sock = -1;
          break;
        }

        if (!RecvFrameData(new_sock, msg)) {
          command = Jt808FrameParse(msg, propara);
          if (command != UP_AUTHENTICATION) {
            close(new_sock);
            new_sock = -1;
            break;
          }
        }
      case UP_AUTHENTICATION:
        memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        msg.size = Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
        SendFrameData(new_sock, msg);
        if (propara.respond_result != kSuccess) {
          close(new_sock);
          new_sock = -1;
        } else if (!device_list_.empty()) {
          for (auto &device : device_list_) {
            BcdFromStringCompress(device.phone_num,
                                  phone_num, strlen(device.phone_num));
            if (memcmp(phone_num, propara.phone_num, 6) == 0) {
              memcpy(device.manufacturer_id, propara.manufacturer_id, 5);
              device.socket_fd = new_sock;
              break;
            }
          }
        }
        break;
      default:
        close(new_sock);
        new_sock = -1;
        break;
    }
  }

  if (new_sock > 0) {
    EpollRegister(epoll_fd_, new_sock);
  }

  return new_sock;
}

void Jt808Service::Run(const int &time_out) {
  int ret = -1;
  int i;
  int active_count;
  char recv_buff[65536] = {0};
  ProtocolParameters propara = {0};
  Message msg = {0};

  while (1) {
    ret = Jt808ServiceWait(time_out);
    if (ret == 0) {  // epoll time out.
      continue;
    } else {
      active_count = ret;
      for (i = 0; i < active_count; ++i) {
        if (epoll_events_[i].data.fd == listen_sock_) {
          if (epoll_events_[i].events & EPOLLIN) {
            AcceptNewClient();
          }
        } else if (epoll_events_[i].data.fd == socket_fd_) {
          if (epoll_events_[i].events & EPOLLIN) {
            AcceptNewCommandClient();
            EpollRegister(epoll_fd_, client_fd_);
          }
        } else if (epoll_events_[i].data.fd == client_fd_) {
          memset(recv_buff, 0x0, sizeof(recv_buff));
          if ((ret = recv(client_fd_, recv_buff, sizeof(recv_buff), 0)) > 0) {
            if (ParseCommand(recv_buff) >= 0) {
               send(client_fd_, recv_buff, strlen(recv_buff), 0);
            }
          }
          close(client_fd_);
        } else if ((epoll_events_[i].events & EPOLLIN) &&
                   !device_list_.empty()) {
          for (auto &device : device_list_) {
            if (epoll_events_[i].data.fd == device.socket_fd) {
              if (RecvFrameData(epoll_events_[i].data.fd, msg) == 0) {
                int cmd = Jt808FrameParse(msg, propara);
                switch (cmd) {
                  case UP_HEARTBEAT:
                  case UP_UPDATERESULT:
                  case UP_POSITIONREPORT:
                    memset(msg.buffer, 0x0, sizeof(msg.buffer));
                    msg.size = Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
                    if (SendFrameData(epoll_events_[i].data.fd, msg)) {
                      close(epoll_events_[i].data.fd);
                      device.socket_fd = -1;
                    }
                    break;
                  default:
                    break;
                }
              } else {
                close(epoll_events_[i].data.fd);
                device.socket_fd = -1;
              }
              break;
            }
          }
        }
      }
    }
  }
}

int Jt808Service::SendFrameData(const int &fd, const Message &msg) {
  int ret = -1;

  signal(SIGPIPE, SIG_IGN);
  ret = send(fd, msg.buffer, msg.size, 0);
  if (ret < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      ret = 0;
    } else {
      if (errno == EPIPE) {
        printf("%s[%d]: remote socket close!!!\n", __FUNCTION__, __LINE__);
      }
      ret = -1;
      printf("%s[%d]: send data failed!!!\n", __FUNCTION__, __LINE__);
    }
  } else if (ret == 0) {
    printf("%s[%d]: connection disconect!!!\n", __FUNCTION__, __LINE__);
    ret = -1;
  }

  return ret >= 0 ? 0 : ret;
}

int Jt808Service::RecvFrameData(const int &fd, Message &msg) {
  int ret = -1;

  memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  ret = recv(fd, msg.buffer, MAX_PROFRAMEBUF_LEN, 0);
  if (ret < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      ret = 0;
    } else {
      ret = -1;
      printf("%s[%d]: recv data failed!!!\n", __FUNCTION__, __LINE__);
    }
    msg.size = 0;
  } else if (ret == 0) {
    ret = -1;
    msg.size = 0;
    printf("%s[%d]: connection disconect!!!\n", __FUNCTION__, __LINE__);
  } else {
    msg.size = ret;
  }

  return ret >= 0 ? 0 : ret;
}

int Jt808Service::Jt808FramePack(Message &msg,
                                 const uint16_t &command,
                                 const ProtocolParameters &propara) {
  uint8_t *msg_body;
  uint8_t u8val;
  uint16_t u16val;
  uint32_t u32val;
  MessageHead *msghead_ptr;

  msghead_ptr = reinterpret_cast<MessageHead *>(&msg.buffer[1]);
  msghead_ptr->id = EndianSwap16(command);
  msghead_ptr->attribute.value = 0;
  msghead_ptr->attribute.bit.encrypt = 0;
  message_flow_num_++;
  msghead_ptr->msgflownum = EndianSwap16(message_flow_num_);
  msghead_ptr->attribute.bit.msglen = 0;
  memcpy(msghead_ptr->phone, propara.phone_num, 6);
  msghead_ptr->attribute.bit.package = 0;
  msg_body = &msg.buffer[MSGBODY_NOPACKAGE_POS];
  msg.size = 13;

  switch (command) {
    case DOWN_UNIRESPONSE:
      u16val = EndianSwap16(propara.respond_flow_num);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      u16val = EndianSwap16(propara.respond_id);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      *msg_body = propara.respond_result;
      msg_body++;
      msg.size += 5;
      msghead_ptr->attribute.bit.msglen += 5;
      break;
    case DOWN_REGISTERRESPONSE:
      u16val = EndianSwap16(propara.respond_flow_num);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      *msg_body = propara.respond_result;
      msg_body++;
      if (propara.respond_result == kSuccess) {
        // send authen code if register success.
        memcpy(msg_body, propara.authen_code, 4);
        msg_body += 4;
        msghead_ptr->attribute.bit.msglen += 7;
        msg.size += 7;
      } else {
        msghead_ptr->attribute.bit.msglen += 3;
        msg.size += 3;
      }
      break;
    case DOWN_SETTERMPARA:
      if (propara.packet_total_num > 1) {
        msghead_ptr->attribute.bit.package = 1;
        u16val = EndianSwap16(propara.packet_total_num);
        memcpy(&msg.buffer[13], &u16val, 2);
        u16val = EndianSwap16(propara.packet_sequence_num);
        memcpy(&msg.buffer[15], &u16val, 2);
        msg_body += 4;
        msg.size += 4;
      }
      msg_body[0] = propara.terminal_parameter_map->size();
      msg_body++;
      msg.size++;
      msghead_ptr->attribute.bit.msglen += 1;
      for (auto &parameter : *propara.terminal_parameter_map) {
        u32val = EndianSwap32(parameter.first);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        switch (GetParameterTypeByParameterId(parameter.first)) {
          case kByteType:
            *msg_body = 1;
            u32val = atoi(parameter.second.c_str());
            memcpy(&msg_body[1], &u32val, 1);
            u8val = 2;
            break;
          case kWordType:
            *msg_body = 2;
            u32val = atoi(parameter.second.c_str());
            u16val = EndianSwap16(u16val);
            memcpy(&msg_body[1], &u16val, 2);
            u8val = 3;
            break;
          case kDwordType:
            *msg_body = 4;
            u32val = atoi(parameter.second.c_str());
            u32val = EndianSwap32(u32val);
            memcpy(&msg_body[1], &u32val, 4);
            u8val = 5;
            break;
          case kStringType:
            *msg_body = parameter.second.size();
            memcpy(&msg_body[1], parameter.second.c_str(),
                   parameter.second.size());
            u8val = 1 + parameter.second.size();
            break;
          default:
            u8val = 0;
            break;
        }
        msg_body += u8val;
        msg.size += 4 + u8val;
        msghead_ptr->attribute.bit.msglen += 4 + u8val;
      }
      break;
    case DOWN_GETTERMPARA:
      break;
    case DOWN_GETSPECTERMPARA:
      *msg_body = propara.terminal_parameter_id_count;
      msg_body++;
      msg.size++;
      msghead_ptr->attribute.bit.msglen += 1;
      // deal terminal parameters id.
      for (int i = 0; i < propara.terminal_parameter_id_count; ++i) {
        memcpy(msg_body, propara.terminal_parameter_id_buffer + i*4, 4);
        msg_body += 4;
        msg.size += 4;
        msghead_ptr->attribute.bit.msglen += 4;
      }
      break;
    case DOWN_TERMINALCONTROL:
      *msg_body = propara.terminal_control_type;
      msg_body++;
      msg.size++;
      msghead_ptr->attribute.bit.msglen += 1;
      break;
    case DOWN_UPDATEPACKAGE:
      if (propara.packet_total_num > 1) {
        msghead_ptr->attribute.bit.package = 1;
        u16val = EndianSwap16(propara.packet_total_num);
        memcpy(&msg.buffer[13], &u16val, 2);
        u16val = EndianSwap16(propara.packet_sequence_num);
        memcpy(&msg.buffer[15], &u16val, 2);
        msg_body += 4;
        msg.size += 4;
      }
      *msg_body = propara.upgrade_type;
      msg_body++;
      memcpy(msg_body, propara.manufacturer_id, 5);
      msg_body += 5;
      msg.size += 6;
      *msg_body = propara.version_num_len;
      msg_body++;
      msg.size++;
      memcpy(msg_body, propara.version_num, propara.version_num_len);
      msg_body += propara.version_num_len;
      msg.size += propara.version_num_len;
      u32val = EndianSwap32(propara.packet_data_len);
      memcpy(msg_body, &u32val, 4);
      msg_body += 4;
      msg.size += 4;
      // content of the upgrade file.
      memcpy(msg_body, propara.packet_data, propara.packet_data_len);
      msg_body += propara.packet_data_len;
      msg.size += propara.packet_data_len;
      msghead_ptr->attribute.bit.msglen += 11 + propara.version_num_len +
                                          propara.packet_data_len;
      break;
    case DOWN_GETPOSITIONINFO:
      break;
    case DOWN_POSITIONTRACK:
      u16val = EndianSwap16(propara.report_interval);
      memcpy(msg_body, &u16val, 2);
      msg_body += 2;
      u32val = EndianSwap32(propara.report_valid_time);
      memcpy(msg_body, &u32val, 4);
      msg_body += 4;
      msg.size += 6;
      msghead_ptr->attribute.bit.msglen += 6;
      break;
    case DOWN_VEHICLECONTROL:
      *msg_body = propara.vehicle_control_flag.value;
      msg_body++;
      msg.size++;
      msghead_ptr->attribute.bit.msglen++;
      break;
    case DOWN_SETCIRCULARAREA:
      *msg_body = propara.set_area_route_type;
      msg_body++;
      *msg_body = propara.circular_area_list->size();
      msg_body++;
      msg.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
      for (auto circular_area : *propara.circular_area_list) {
        u32val = EndianSwap32(circular_area->area_id);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u16val = EndianSwap16(circular_area->area_attribute.value);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        u32val = EndianSwap32(circular_area->center_point.latitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u32val = EndianSwap32(circular_area->center_point.longitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u32val = EndianSwap32(circular_area->radius);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        msg.size += 18;
        msghead_ptr->attribute.bit.msglen += 18;
        if (circular_area->area_attribute.bit.bytime) {
          memcpy(msg_body, circular_area->start_time, 6);
          msg_body += 6;
          memcpy(msg_body, circular_area->end_time, 6);
          msg_body += 6;
          msg.size += 12;
          msghead_ptr->attribute.bit.msglen += 12;
        }
        if (circular_area->area_attribute.bit.speedlimit) {
          u16val = EndianSwap16(circular_area->max_speed);
          memcpy(msg_body, &u16val, 2);
          msg_body += 2;
          *msg_body++ = circular_area->overspeed_duration;
          msg.size += 3;
          msghead_ptr->attribute.bit.msglen += 3;
        }
      }
      ClearContainerElement(propara.circular_area_list);
      delete propara.circular_area_list;
      break;
    case DOWN_SETRECTANGLEAREA:
      *msg_body = propara.set_area_route_type;
      msg_body++;
      *msg_body = propara.rectangle_area_list->size();
      msg_body++;
      msg.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
      for (auto rectangle_area : *propara.rectangle_area_list) {
        u32val = EndianSwap32(rectangle_area->area_id);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u16val = EndianSwap16(rectangle_area->area_attribute.value);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        u32val = EndianSwap32(rectangle_area->upper_left_corner.latitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u32val = EndianSwap32(rectangle_area->upper_left_corner.longitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u32val = EndianSwap32(rectangle_area->bottom_right_corner.latitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u32val = EndianSwap32(rectangle_area->bottom_right_corner.longitude);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        msg.size += 22;
        msghead_ptr->attribute.bit.msglen += 12;
        if (rectangle_area->area_attribute.bit.bytime) {
          memcpy(msg_body, rectangle_area->start_time, 6);
          msg_body += 6;
          memcpy(msg_body, rectangle_area->end_time, 6);
          msg_body += 6;
          msg.size += 12;
          msghead_ptr->attribute.bit.msglen += 12;
        }
        if (rectangle_area->area_attribute.bit.speedlimit) {
          u16val = EndianSwap16(rectangle_area->max_speed);
          memcpy(msg_body, &u16val, 2);
          msg_body += 2;
          *msg_body++ = rectangle_area->overspeed_duration;
          msg.size += 3;
          msghead_ptr->attribute.bit.msglen += 3;
        }
      }
      ClearContainerElement(propara.rectangle_area_list);
      delete propara.rectangle_area_list;
      break;
    case DOWN_SETPOLYGONALAREA:
      *msg_body = propara.set_area_route_type;
      msg_body++;
      *msg_body = propara.polygonal_area_list->size();
      msg_body++;
      msg.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
      for (auto polygonal_area : *propara.polygonal_area_list) {
        u32val = EndianSwap32(polygonal_area->area_id);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u16val = EndianSwap16(polygonal_area->area_attribute.value);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        msg.size += 6;
        msghead_ptr->attribute.bit.msglen += 6;
        if (polygonal_area->area_attribute.bit.bytime) {
          memcpy(msg_body, polygonal_area->start_time, 6);
          msg_body += 6;
          memcpy(msg_body, polygonal_area->end_time, 6);
          msg_body += 6;
          msg.size += 12;
          msghead_ptr->attribute.bit.msglen += 12;
        }
        if (polygonal_area->area_attribute.bit.speedlimit) {
          u16val = EndianSwap16(polygonal_area->max_speed);
          memcpy(msg_body, &u16val, 2);
          msg_body += 2;
          *msg_body++ = polygonal_area->overspeed_duration;
          msg.size += 3;
          msghead_ptr->attribute.bit.msglen += 3;
        }
        u16val = EndianSwap16(polygonal_area->coordinate_count);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        msg.size += 2;
        msghead_ptr->attribute.bit.msglen += 2;
        for (auto coordinate : *polygonal_area->coordinate_list) {
          u32val = EndianSwap32(coordinate->latitude);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          u32val = EndianSwap32(coordinate->longitude);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          msg.size += 8;
          msghead_ptr->attribute.bit.msglen += 8;
        }
        ClearContainerElement(polygonal_area->coordinate_list);
        delete polygonal_area->coordinate_list;
      }
      ClearContainerElement(propara.polygonal_area_list);
      delete propara.polygonal_area_list;
      break;
    case DOWN_SETROUTE:
      *msg_body = propara.set_area_route_type;
      msg_body++;
      *msg_body = propara.route_list->size();
      msg_body++;
      msg.size += 2;
      msghead_ptr->attribute.bit.msglen += 2;
      for (auto route : *propara.route_list) {
        u32val = EndianSwap32(route->route_id);
        memcpy(msg_body, &u32val, 4);
        msg_body += 4;
        u16val = EndianSwap16(route->route_attribute.value);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        msg.size += 6;
        msghead_ptr->attribute.bit.msglen += 6;
        if (route->route_attribute.bit.bytime) {
          memcpy(msg_body, route->start_time, 6);
          msg_body += 6;
          memcpy(msg_body, route->end_time, 6);
          msg_body += 6;
          msg.size += 12;
          msghead_ptr->attribute.bit.msglen += 12;
        }
        u16val = EndianSwap16(route->inflection_point_count);
        memcpy(msg_body, &u16val, 2);
        msg_body += 2;
        msg.size += 2;
        msghead_ptr->attribute.bit.msglen += 2;
        for (auto inflection_point : *route->inflection_point_list) {
          u32val = EndianSwap32(inflection_point->inflection_point_id);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          u32val = EndianSwap32(inflection_point->road_section_id);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          u32val = EndianSwap32(inflection_point->coordinate.latitude);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          u32val = EndianSwap32(inflection_point->coordinate.longitude);
          memcpy(msg_body, &u32val, 4);
          msg_body += 4;
          *msg_body++ = inflection_point->road_section_wide;
          *msg_body++ = inflection_point->road_section_attribute.value;
          msg.size += 18;
          msghead_ptr->attribute.bit.msglen += 18;
          if (inflection_point->road_section_attribute.bit.traveltime) {
            u16val = EndianSwap16(inflection_point->max_driving_time);
            memcpy(msg_body, &u16val, 2);
            msg_body += 2;
            u16val = EndianSwap16(inflection_point->min_driving_time);
            memcpy(msg_body, &u16val, 2);
            msg_body += 2;
            msg.size += 4;
            msghead_ptr->attribute.bit.msglen += 4;
          }
          if (inflection_point->road_section_attribute.bit.speedlimit) {
            u16val = EndianSwap16(inflection_point->max_speed);
            memcpy(msg_body, &u16val, 2);
            msg_body += 2;
            *msg_body++ = inflection_point->overspeed_duration;
            msg.size += 3;
            msghead_ptr->attribute.bit.msglen += 3;
          }
        }
        ClearContainerElement(route->inflection_point_list);
        delete route->inflection_point_list;
      }
      ClearContainerElement(propara.route_list);
      delete propara.route_list;
      break;
    case DOWN_DELCIRCULARAREA:
    case DOWN_DELRECTANGLEAREA:
    case DOWN_DELPOLYGONALAREA:
    case DOWN_DELROUTE:
      *msg_body++ = propara.area_route_id_count;
      msg.size += 1;
      msghead_ptr->attribute.bit.msglen += 1;
      for (int i = 0; i < propara.area_route_id_count; ++i) {
        memcpy(msg_body, propara.area_route_id_buffer + i*4, 4);
        msg_body += 4;
        msg.size += 4;
        msghead_ptr->attribute.bit.msglen += 4;
      }
      break;
    case DOWN_PASSTHROUGH:
      *msg_body = propara.pass_through->type;
      msg_body++;
      msg.size++;
      memcpy(msg_body, propara.pass_through->buffer,
             propara.pass_through->size);
      msg.size += propara.pass_through->size;
      msghead_ptr->attribute.bit.msglen += propara.pass_through->size;
      break;
    default:
      break;
  }
  u16val = msghead_ptr->attribute.value;
  msghead_ptr->attribute.value = EndianSwap16(u16val);

  *msg_body = BccCheckSum(&msg.buffer[1], msg.size - 1);
  msg.size++;

  msg.size = Escape(msg.buffer + 1, msg.size);
  msg.buffer[0] = PROTOCOL_SIGN;
  msg.buffer[msg.size++] = PROTOCOL_SIGN;

  printf("%s[%d]: socket-send[%lu]:\n", __FUNCTION__, __LINE__, msg.size);
  for (uint16_t i = 0; i < msg.size; ++i) {
    printf("%02X ", msg.buffer[i]);
  }
  printf("\r\n");

  return msg.size;
}

uint16_t Jt808Service::Jt808FrameParse(Message &msg,
                                       ProtocolParameters &propara) {
  uint8_t *msg_body;
  char phone_num[12] = {0};
  uint8_t u8val;
  uint16_t u16val;
  uint32_t u32val;
  MessageHead *msghead_ptr;
  MessageBodyAttr msgbody_attribute;

  // printf("%s[%d]: socket-recv[%lu]:\n", __FUNCTION__, __LINE__, msg.size);
  // for (uint16_t i = 0; i < msg.size; ++i) {
  //   printf("%02X ", msg.buffer[i]);
  // }
  // printf("\r\n");

  msg.size = ReverseEscape(&msg.buffer[1], msg.size);
  msghead_ptr = reinterpret_cast<MessageHead *>(&msg.buffer[1]);
  u16val = msghead_ptr->attribute.value;
  msgbody_attribute.value = EndianSwap16(u16val);
  if (msgbody_attribute.bit.package) {
    msg_body = &msg.buffer[MSGBODY_PACKAGE_POS];
  } else {
    msg_body = &msg.buffer[MSGBODY_NOPACKAGE_POS];
  }

  u16val = msghead_ptr->msgflownum;
  propara.respond_flow_num = EndianSwap16(u16val);
  u16val = msghead_ptr->id;
  uint16_t message_id = EndianSwap16(u16val);
  propara.respond_id = message_id;
  switch (message_id) {
    case UP_UNIRESPONSE:
      memcpy(&u16val, &msg_body[2], 2);
      propara.respond_id = EndianSwap16(u16val);
      switch(propara.respond_id) {
        case DOWN_UPDATEPACKAGE:
          printf("%s[%d]: received updatepackage respond: ",
                 __FUNCTION__, __LINE__);
          if (propara.packet_total_num && (msg_body[4] == kSuccess)) {
            propara.packet_response_success_num++;
          }
          break;
        case DOWN_SETTERMPARA:
          printf("%s[%d]: received set terminal parameter respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_TERMINALCONTROL:
          printf("%s[%d]: received terminal control respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_POSITIONTRACK:
          printf("%s[%d]: received position track respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_SETCIRCULARAREA:
          printf("%s[%d]: received set circular area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_DELCIRCULARAREA:
          printf("%s[%d]: received delete circular area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_SETRECTANGLEAREA:
          printf("%s[%d]: received set rectangle area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_DELRECTANGLEAREA:
          printf("%s[%d]: received delete rectangle area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_SETPOLYGONALAREA:
          printf("%s[%d]: received set polygonal area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_DELPOLYGONALAREA:
          printf("%s[%d]: received delete polygonal area respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_SETROUTE:
          printf("%s[%d]: received set route respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_DELROUTE:
          printf("%s[%d]: received delete route respond: ",
                 __FUNCTION__, __LINE__);
          break;
        case DOWN_PASSTHROUGH:
          printf("%s[%d]: received down passthrough respond: ",
                 __FUNCTION__, __LINE__);
          break;
        default:
          break;
      }
      // Respond result.
      if (msg_body[4] == kSuccess) {
        printf("normal\r\n");
      } else if (msg_body[4] == kFailure) {
        printf("failed\r\n");
      } else if (msg_body[4] == kMessageHasWrong) {
        printf("message has something wrong\r\n");
      } else if (msg_body[4] == kNotSupport) {
        printf("message not support\r\n");
      }
      break;
    case UP_HEARTBEAT:
      printf("%s[%d]: received heartbeat: ", __FUNCTION__, __LINE__);
      break;
    case UP_REGISTER:
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      propara.respond_result = kNoSuchVehicleInTheDatabase;
      if (!device_list_.empty()) {
        propara.respond_result = kNoSuchTerminalInTheDatabase;
        for (auto &device : device_list_) {
          BcdFromStringCompress(device.phone_num, phone_num,
                                strlen(device.phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0) {
            if (device.socket_fd == -1) {
              memcpy(propara.authen_code, device.authen_code, 4);
              memcpy(propara.manufacturer_id, &msg_body[4], 5);
              propara.respond_result = kRegisterSuccess;
            } else {
              propara.respond_result = kTerminalHaveBeenRegistered;
            }
            break;
          }
        }
      }
      break;
    case UP_AUTHENTICATION:
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      propara.respond_result = kFailure;
      if (!device_list_.empty()) {
        for (auto &device : device_list_) {
          BcdFromStringCompress(device.phone_num, phone_num,
                                strlen(device.phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0) {
            if ((memcmp(device.authen_code, msg_body,
                    msgbody_attribute.bit.msglen) == 0)) {
              propara.respond_result = kSuccess;
              break;
            }
          }
        }
      }
      break;
    case UP_GETPARARESPONSE:
      printf("%s[%d]: received get terminal parameter respond\n",
             __FUNCTION__, __LINE__);
      char parameter_value[256];
      uint32_t parameter_id;
      int parameter_count;
      if (msgbody_attribute.bit.package) {
        memcpy(&u16val, &msg.buffer[13], 2);
        propara.packet_total_num = EndianSwap16(u16val);
        memcpy(&u16val, &msg.buffer[15], 2);
        propara.packet_sequence_num = EndianSwap16(u16val);
      }
      parameter_count = msg_body[2];
      msg_body += 3;
      while (parameter_count--) {
        memcpy(&u32val, &msg_body[0], 4);
        parameter_id = EndianSwap32(u32val);
        msg_body += 4;
        u8val = msg_body[0];
        msg_body++;
        memset(parameter_value, 0x0, sizeof(parameter_value));
        switch (GetParameterTypeByParameterId(parameter_id)) {
          case kWordType:
            memcpy(&u16val, &msg_body[0], u8val);
            u16val = EndianSwap16(u16val);
            memcpy(parameter_value, &u16val, u8val);
            break;
          case kDwordType:
            memcpy(&u32val, &msg_body[0], u8val);
            u32val = EndianSwap32(u32val);
            memcpy(parameter_value, &u32val, u8val);
            break;
          case kByteType:
          case kStringType:
            memcpy(parameter_value, &msg_body[0], u8val);
            break;
          default:
            break;
        }
        msg_body += u8val;
        propara.terminal_parameter_map->insert(
            std::make_pair(parameter_id, parameter_value));
      }
      propara.respond_result = kSuccess;
      break;
    case UP_UPDATERESULT:
      printf("%s[%d]: received update result: ", __FUNCTION__, __LINE__);
      if (msg_body[4] == 0x00) {
        printf("normal\r\n");
      } else if (msg_body[4] == 0x01) {
        printf("failed\r\n");
      } else if (msg_body[4] == 0x02) {
        printf("message has something wrong\r\n");
      } else if (msg_body[4] == 0x03) {
        printf("message not support\r\n");
      }
      propara.respond_result = kSuccess;
      break;
    case UP_GETPOSITIONINFORESPONSE:
      printf("%s[%d]: received get position info:\n", __FUNCTION__, __LINE__);
      ParsePositionReport(msg.buffer, msg.size, 2);
      propara.respond_result = kSuccess;
      break;
    case UP_POSITIONREPORT:
      printf("%s[%d]: received position report:\n", __FUNCTION__, __LINE__);
      ParsePositionReport(msg.buffer, msg.size, 0);
      propara.respond_result = kSuccess;
      break;
    case UP_VEHICLECONTROLRESPONSE:
      printf("%s[%d]: received vehicle control:\n", __FUNCTION__, __LINE__);
      ParsePositionReport(msg.buffer, msg.size, 2);
      propara.respond_result = kSuccess;
      break;
    case UP_PASSTHROUGH:
      printf("%s[%d]: received up passthrough\n", __FUNCTION__, __LINE__);
      if (propara.pass_through == nullptr) {
        propara.pass_through = new PassThrough;
        memset(propara.pass_through, 0x0, sizeof(PassThrough));
      }
      propara.pass_through->type = *msg_body;
      msg_body++;
      propara.pass_through->size = msgbody_attribute.bit.msglen - 1;
      memcpy(propara.pass_through->buffer,
             msg_body, propara.pass_through->size);
      propara.respond_result = kSuccess;
      break;
    case UP_CANBUSDATAUPLOAD:
      printf("%s[%d]: received up can bus data:\n", __FUNCTION__, __LINE__);
      if (propara.can_bus_data_list == nullptr) {
        propara.can_bus_data_list = new std::vector<CanBusData*>;
      }
      memcpy(&u16val, &msg_body[0], 2);
      msg_body += 2;
      u16val = EndianSwap16(u16val);
      if (u16val > 0) {
        uint16_t max_item_count = u16val;
        u8val = HexFromBcd(msg_body[0]);
        propara.can_bus_data_timestamp.hour = u8val;
        u8val = HexFromBcd(msg_body[1]);
        propara.can_bus_data_timestamp.minute = u8val;
        u8val = HexFromBcd(msg_body[2]);
        propara.can_bus_data_timestamp.second = u8val;
        u8val = HexFromBcd(msg_body[3]);
        u16val = u8val * 10;
        u8val = HexFromBcd(msg_body[4]);
        u16val += u8val;
        propara.can_bus_data_timestamp.millisecond = u16val;
        msg_body += 5;
        CanBusData *item;
        while (max_item_count--) {
          item = new CanBusData;
          memcpy(&(item->can_id), &msg_body[0], 4);
          msg_body += 4;
          memcpy(item->buffer, &msg_body[0], 8);
          msg_body += 8;
          propara.can_bus_data_list->push_back(item);
        }
        fprintf(stdout, "\tcount: %lu\n"
                        "\ttimestamp: %02d:%02d:%02d%04d\n",
                propara.can_bus_data_list->size(),
                propara.can_bus_data_timestamp.hour,
                propara.can_bus_data_timestamp.minute,
                propara.can_bus_data_timestamp.second,
                propara.can_bus_data_timestamp.millisecond);
        ClearContainerElement(propara.can_bus_data_list);
        delete propara.can_bus_data_list;
      }
      break;
    case DOWN_PACKETRESEND:
      msg_body += 2;
      u8val = *msg_body;
      msg_body++;
      for (int i = 0; i < u8val; ++i) {
        memcpy(&u16val, &msg_body[0], 2);
        u16val = EndianSwap16(u16val);
        propara.packet_id_list->push_back(u16val);
        msg_body += 2;
      }
      propara.respond_result = kSuccess;
      break;
    default:
      break;
  }

  return message_id;
}

int Jt808Service::DealGetTerminalParameterRequest(
                      DeviceNode &device, std::vector<std::string> &va_vec) {
  int retval = -1;
  uint16_t u16val;
  uint32_t u32val;
  uint32_t parameter_id;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  PreparePhoneNum(device.phone_num, propara.phone_num);
  propara.terminal_parameter_id_count = 0;
  if (va_vec.empty()) {
    Jt808FramePack(msg, DOWN_GETTERMPARA, propara);
  } else {
    propara.terminal_parameter_id_buffer = new uint8_t [va_vec.size() * 4];
    uint8_t *ptr = propara.terminal_parameter_id_buffer;
    while (!va_vec.empty()) {
      arg = va_vec.back();
      va_vec.pop_back();
      sscanf(arg.c_str(), "%X", &u32val);
      parameter_id = EndianSwap32(u32val);
      memcpy(ptr, &parameter_id, 4);
      ptr += 4;
      propara.terminal_parameter_id_count++;
    }
    Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
    delete [] propara.terminal_parameter_id_buffer;
  }

  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    if (propara.terminal_parameter_map == nullptr) {
      propara.terminal_parameter_map = new std::map<uint32_t, std::string>;
    }
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) == UP_GETPARARESPONSE) {
          memset(&msg, 0x0, sizeof(msg));
          Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
          if (SendFrameData(device.socket_fd, msg)) {
            close(device.socket_fd);
            device.socket_fd = -1;
            break;
          }
          if (propara.packet_total_num != propara.packet_sequence_num) {
            continue;
          }
          char parameter_s[512] = {0};
          for (auto &parameter : *propara.terminal_parameter_map) {
            memset(parameter_s, 0x0, sizeof(parameter_s));
            u16val = GetParameterTypeByParameterId(parameter.first);
            if (u16val == kStringType) {
              snprintf(parameter_s, sizeof(parameter_s), "%04X:%s",
                       parameter.first, parameter.second.c_str());
            } else {
              u32val = 0;
              memcpy(&u32val, parameter.second.c_str(),
                     parameter.second.size());
              snprintf(parameter_s, sizeof(parameter_s), "%04X:%u",
                       parameter.first, u32val);
            }
            arg = parameter_s;
            va_vec.push_back(arg);
          }
          reverse(va_vec.begin(), va_vec.end());
          retval = 0;
          break;
        }
      }
    }
  }
  propara.terminal_parameter_map->clear();
  delete propara.terminal_parameter_map;
  propara.terminal_parameter_map = nullptr;
  return retval;
}

int Jt808Service::DealSetTerminalParameterRequest(
                      DeviceNode &device, std::vector<std::string> &va_vec) {
  int retval = 0;
  char value[256] = {0};
  uint16_t data_len = 0;
  uint32_t u32val = 0;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  auto va_it = va_vec.begin();
  while (va_it != va_vec.end()) {
    data_len += 5 + va_it->size();
    ++va_it;
  }

  if (data_len > 1022) {
    propara.packet_total_num = data_len/1022 + 1;
    propara.packet_sequence_num = 1;
  }

  if (propara.terminal_parameter_map == nullptr) {
    propara.terminal_parameter_map = new std::map<uint32_t, std::string>;
  }

  while (1) {
    data_len = 0;
    while (!va_vec.empty()) {
      arg = va_vec.back();
      memset(value, 0x0, sizeof(value));
      sscanf(arg.c_str(), "%x:%s", &u32val, value);
      data_len += 5 + strlen(value);
      if (data_len > 1022) break;
      propara.terminal_parameter_map->insert(std::make_pair(u32val, value));
      va_vec.pop_back();
    }
    if (propara.terminal_parameter_map->empty()) {
      return retval;
    }
    PreparePhoneNum(device.phone_num, propara.phone_num);
    Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
    SendFrameData(device.socket_fd, msg);
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) &&
            (propara.respond_id == DOWN_SETTERMPARA)) {
          break;
        }
      }
    }
    propara.terminal_parameter_map->clear();
    if (va_vec.empty()) {
      break;
    }
    if (propara.packet_total_num > propara.packet_sequence_num) {
      ++propara.packet_sequence_num;
    }
  }
  return retval;
}

int Jt808Service::DealSetCircularAreaRequest(
                      DeviceNode &device, std::vector<std::string> &va_vec) {
  int retval = 0;
  uint32_t u32val;
  double doubleval;
  char time[6] = {0};

  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  propara.circular_area_list = new std::vector<CircularArea*>;
  arg = va_vec.back();
  if (arg == "update") {
    propara.set_area_route_type = 0;
  } else if (arg == "append") {
    propara.set_area_route_type = 1;
  } else if (arg == "modify") {
    propara.set_area_route_type = 2;
  }
  va_vec.pop_back();
  CircularArea *area;
  while (!va_vec.empty()) {
    area = new CircularArea;
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_id = u32val;
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_attribute.value = static_cast<uint16_t>(u32val);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->center_point.latitude = static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->center_point.longitude = static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%u", &area->radius);
    va_vec.pop_back();
    if (area->area_attribute.bit.bytime) {
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->start_time, time, 6);
      va_vec.pop_back();
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->end_time, time, 6);
      va_vec.pop_back();
    }
    if (area->area_attribute.bit.speedlimit) {
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->max_speed = u32val;
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->overspeed_duration = static_cast<uint8_t>(u32val);
      va_vec.pop_back();
    }
    propara.circular_area_list->push_back(area);
  }

  if (propara.circular_area_list->empty()) {
    return retval;
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETCIRCULARAREA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      break;
    } else if (msg.size > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETCIRCULARAREA)) {
        break;
      }
    }
  }
  return retval;
}

int Jt808Service::DealSetRectangleAreaRequest(
                      DeviceNode &device, std::vector<std::string> &va_vec) {
  int retval = 0;
  uint32_t u32val;
  double doubleval;
  char time[6] = {0};

  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  propara.rectangle_area_list = new std::vector<RectangleArea*>;
  arg = va_vec.back();
  if (arg == "update") {
    propara.set_area_route_type = 0;
  } else if (arg == "append") {
    propara.set_area_route_type = 1;
  } else if (arg == "modify") {
    propara.set_area_route_type = 2;
  }
  va_vec.pop_back();
  RectangleArea *area;
  while (!va_vec.empty()) {
    area = new RectangleArea;
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_id = u32val;
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_attribute.value = static_cast<uint16_t>(u32val);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->upper_left_corner.latitude =
        static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->upper_left_corner.longitude =
        static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->bottom_right_corner.latitude =
        static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%lf", &doubleval);
    area->bottom_right_corner.longitude =
        static_cast<uint32_t>(doubleval * 1000000UL);
    va_vec.pop_back();
    if (area->area_attribute.bit.bytime) {
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->start_time, time, 6);
      va_vec.pop_back();
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->end_time, time, 6);
      va_vec.pop_back();
    }
    if (area->area_attribute.bit.speedlimit) {
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->max_speed = u32val;
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->overspeed_duration = static_cast<uint8_t>(u32val);
      va_vec.pop_back();
    }
    propara.rectangle_area_list->push_back(area);
  }

  if (propara.rectangle_area_list->empty()) {
    return retval;
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETRECTANGLEAREA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      break;
    } else if (msg.size > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETRECTANGLEAREA)) {
        break;
      }
    }
  }
  return retval;
}

int Jt808Service::DealSetPolygonalAreaRequest(
                      DeviceNode &device, std::vector<std::string> &va_vec) {
  int retval = 0;
  uint32_t u32val;
  double doubleval;
  char time[6] = {0};

  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  propara.polygonal_area_list = new std::vector<PolygonalArea*>;
  arg = va_vec.back();
  if (arg == "update") {
    propara.set_area_route_type = 0;
  } else if (arg == "append") {
    propara.set_area_route_type = 1;
  } else if (arg == "modify") {
    propara.set_area_route_type = 2;
  }
  va_vec.pop_back();
  PolygonalArea *area;
  while (!va_vec.empty()) {
    area = new PolygonalArea;
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_id = u32val;
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    area->area_attribute.value = static_cast<uint16_t>(u32val);
    va_vec.pop_back();
    if (area->area_attribute.bit.bytime) {
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->start_time, time, 6);
      va_vec.pop_back();
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(area->end_time, time, 6);
      va_vec.pop_back();
    }
    if (area->area_attribute.bit.speedlimit) {
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->max_speed = u32val;
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      area->overspeed_duration = static_cast<uint8_t>(u32val);
      va_vec.pop_back();
    }
    arg = va_vec.back();
    sscanf(arg.c_str(), "%u", &u32val);
    area->coordinate_count = static_cast<uint8_t>(u32val);
    va_vec.pop_back();
    area->coordinate_list = new std::vector<Coordinate*>;
    Coordinate *coordinate;
    for (int i = 0; i < area->coordinate_count; ++i) {
      coordinate = new Coordinate;
      arg = va_vec.back();
      sscanf(arg.c_str(), "%lf", &doubleval);
      coordinate->latitude = static_cast<uint32_t>(doubleval * 1000000UL);
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%lf", &doubleval);
      coordinate->longitude = static_cast<uint32_t>(doubleval * 1000000UL);
      va_vec.pop_back();
      area->coordinate_list->push_back(coordinate);
    }
    propara.polygonal_area_list->push_back(area);
  }

  if (propara.polygonal_area_list->empty()) {
    return retval;
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETPOLYGONALAREA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      break;
    } else if (msg.size > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETPOLYGONALAREA)) {
        break;
      }
    }
  }
  return retval;
}

int Jt808Service::DealSetRouteRequest(DeviceNode &device,
                                      std::vector<std::string> &va_vec) {
  int retval = 0;
  uint32_t u32val;
  double doubleval;
  char time[6] = {0};

  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  propara.route_list = new std::vector<Route*>;
  arg = va_vec.back();
  if (arg == "update") {
    propara.set_area_route_type = 0;
  } else if (arg == "append") {
    propara.set_area_route_type = 1;
  } else if (arg == "modify") {
    propara.set_area_route_type = 2;
  }
  va_vec.pop_back();
  Route *route;
  while (!va_vec.empty()) {
    route = new Route;
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    route->route_id = u32val;
    va_vec.pop_back();
    arg = va_vec.back();
    sscanf(arg.c_str(), "%x", &u32val);
    route->route_attribute.value = static_cast<uint16_t>(u32val);
    va_vec.pop_back();
    if (route->route_attribute.bit.bytime) {
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(route->start_time, time, 6);
      va_vec.pop_back();
      arg = va_vec.back();
      BcdFromStringCompress(arg.c_str(), time, arg.length());
      memcpy(route->end_time, time, 6);
      va_vec.pop_back();
    }
    arg = va_vec.back();
    sscanf(arg.c_str(), "%u", &u32val);
    route->inflection_point_count = static_cast<uint8_t>(u32val);
    va_vec.pop_back();
    route->inflection_point_list = new std::vector<InflectionPoint*>;
    InflectionPoint *inflection_point;
    for (int i = 0; i < route->inflection_point_count; ++i) {
      inflection_point = new InflectionPoint;
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      inflection_point->inflection_point_id = u32val;
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      inflection_point->road_section_id = u32val;
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%lf", &doubleval);
      inflection_point->coordinate.latitude =
          static_cast<uint32_t>(doubleval * 1000000UL);
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%lf", &doubleval);
      inflection_point->coordinate.longitude =
          static_cast<uint32_t>(doubleval * 1000000UL);
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      inflection_point->road_section_wide = static_cast<uint8_t>(u32val);
      va_vec.pop_back();
      arg = va_vec.back();
      sscanf(arg.c_str(), "%u", &u32val);
      inflection_point->road_section_attribute.value =
          static_cast<uint8_t>(u32val);
      va_vec.pop_back();
      if (inflection_point->road_section_attribute.bit.traveltime) {
        arg = va_vec.back();
        sscanf(arg.c_str(), "%u", &u32val);
        inflection_point->max_driving_time = static_cast<uint16_t>(u32val);
        va_vec.pop_back();
        arg = va_vec.back();
        sscanf(arg.c_str(), "%u", &u32val);
        inflection_point->min_driving_time = static_cast<uint16_t>(u32val);
        va_vec.pop_back();
      }
      if (inflection_point->road_section_attribute.bit.speedlimit) {
        arg = va_vec.back();
        sscanf(arg.c_str(), "%u", &u32val);
        inflection_point->max_speed = u32val;
        va_vec.pop_back();
        arg = va_vec.back();
        sscanf(arg.c_str(), "%u", &u32val);
        inflection_point->overspeed_duration = static_cast<uint8_t>(u32val);
        va_vec.pop_back();
      }
      route->inflection_point_list->push_back(inflection_point);
    }
    propara.route_list->push_back(route);
  }

  if (propara.route_list->empty()) {
    return retval;
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETROUTE, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      break;
    } else if (msg.size > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETROUTE)) {
        break;
      }
    }
  }
  return retval;
}

int Jt808Service::DealDeleteAreaRouteRequest(DeviceNode &device,
                                             std::vector<std::string> &va_vec,
                                             const uint16_t &command) {
  int retval = -1;
  uint32_t u32val;
  uint32_t area_route_id;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  PreparePhoneNum(device.phone_num, propara.phone_num);
  propara.area_route_id_count = 0;
  if (!va_vec.empty()) {
    propara.area_route_id_buffer = new uint8_t [va_vec.size() * 4];
    uint8_t *ptr = propara.area_route_id_buffer;
    while (!va_vec.empty()) {
      arg = va_vec.back();
      va_vec.pop_back();
      sscanf(arg.c_str(), "%x", &u32val);
      area_route_id = EndianSwap32(u32val);
      memcpy(ptr, &area_route_id, 4);
      ptr += 4;
      propara.area_route_id_count++;
    }
  }
  Jt808FramePack(msg, command, propara);
  delete [] propara.area_route_id_buffer;

  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) && (propara.respond_id == command)) {
            retval = 0;
            break;
        }
      }
    }
  }
  return retval;
}

int Jt808Service::DealGetPositionInfoRequest(DeviceNode &device) {
  int retval = -1;
  ProtocolParameters propara = {0};
  Message msg = {0};

  Jt808FramePack(msg, DOWN_GETPOSITIONINFO, propara);
  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) == UP_GETPOSITIONINFORESPONSE) {
            retval = 0;
            break;
        }
      }
    }
  }
  return retval;
}

int Jt808Service::DealPositionTrackRequest(DeviceNode &device,
                                           std::vector<std::string> &va_vec) {
  int retval = -1;
  uint32_t u32val;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  propara.report_interval = static_cast<uint16_t>(u32val);
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  propara.report_valid_time = u32val;

  Jt808FramePack(msg, DOWN_POSITIONTRACK, propara);
  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) &&
            (propara.respond_id == DOWN_POSITIONTRACK)) {
            retval = 0;
            break;
        }
      }
    }
  }
  return retval;
}

int Jt808Service::DealTerminalControlRequest(DeviceNode &device,
                                             std::vector<std::string> &va_vec) {
  int retval = -1;
  uint32_t u32val;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  propara.terminal_control_type = static_cast<uint8_t>(u32val);

  Jt808FramePack(msg, DOWN_TERMINALCONTROL, propara);
  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) &&
            (propara.respond_id == DOWN_TERMINALCONTROL)) {
            retval = 0;
            break;
        }
      }
    }
  }
  return retval;
}

int Jt808Service::DealVehicleControlRequest(DeviceNode &device,
                                            std::vector<std::string> &va_vec) {
  int retval = -1;
  uint32_t u32val;
  std::string arg;
  ProtocolParameters propara = {0};
  Message msg = {0};

  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%x", &u32val);
  propara.vehicle_control_flag.value = static_cast<uint8_t>(u32val);

  Jt808FramePack(msg, DOWN_VEHICLECONTROL, propara);
  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) == UP_VEHICLECONTROLRESPONSE) {
            retval = 0;
            break;
        }
      }
    }
  }
  return retval;
}

int Jt808Service::ParseCommand(char *buffer) {
  int retval = 0;
  std::string arg;
  std::string phone_num;
  std::stringstream sstr;
  std::vector<std::string> va_vec;

  sstr.clear();
  sstr << buffer;
  do {
    arg.clear();
    sstr >> arg;
    if (arg.empty()) {
      break;
    }
    va_vec.push_back(arg);
  } while (1);
  sstr.str("");
  sstr.clear();
  memset(buffer, 0x0, strlen(buffer));
  reverse(va_vec.begin(), va_vec.end());

  arg = va_vec.back();
  va_vec.pop_back();
  if (!device_list_.empty()) {
    auto device_it = device_list_.begin();
    while (device_it != device_list_.end()) {
      phone_num = device_it->phone_num;
      if (arg == phone_num) {
        break;
      }
      ++device_it;
    }

    if (device_it != device_list_.end() && (device_it->socket_fd > 0)) {
      arg = va_vec.back();
      va_vec.pop_back();
      if (arg == "upgrade") {
        arg = va_vec.back();
        va_vec.pop_back();
        if ((arg == "device") || (arg == "gps") ||
            (arg == "system") || (arg == "cdradio")) {
          if (arg == "device") {
            device_it->upgrade_type = 0x0;
          } else if (arg == "gps") {
            device_it->upgrade_type = 0x34;
          } else if (arg == "cdradio") {
            device_it->upgrade_type = 0x35;
          } else if (arg == "system") {
            device_it->upgrade_type = 0x36;
          } else {
            return -1;
          }

          arg = va_vec.back();
          va_vec.pop_back();
          memset(device_it->upgrade_version,
                 0x0, sizeof(device_it->upgrade_version));
          arg.copy(device_it->upgrade_version, arg.length(), 0);
          arg = va_vec.back();
          va_vec.pop_back();
          memset(device_it->file_path, 0x0, sizeof(device_it->file_path));
          arg.copy(device_it->file_path, arg.length(), 0);
          device_it->has_upgrade = true;
          // start upgrade deal thread.
          std::thread start_upgrade_thread(StartUpgradeThread, this);
          start_upgrade_thread.detach();
          memcpy(buffer, "operation completed.", 20);
        }
      } else if (arg == "getterminalparameter") {
        EpollUnregister(epoll_fd_, device_it->socket_fd);
        retval = DealGetTerminalParameterRequest(*device_it, va_vec);
        if (retval == 0) {
          std::string result = "terminal parameter(id:value): ";
          while (!va_vec.empty()) {
            result += va_vec.back();
            va_vec.pop_back();
            if (va_vec.empty()) {
              break;
            }
            result += ",";
          }
          result.copy(buffer, result.size(), 0);
        }
        EpollRegister(epoll_fd_, device_it->socket_fd);
      } else {
        retval = -1;
        EpollUnregister(epoll_fd_, device_it->socket_fd);
        if (arg == "setterminalparameter") {
          retval = DealSetTerminalParameterRequest(*device_it, va_vec);
        } else if (arg == "setcirculararea") {
          retval = DealSetCircularAreaRequest(*device_it, va_vec);
        } else if (arg == "delcirculararea") {
          retval = DealDeleteAreaRouteRequest(*device_it, va_vec,
                                              DOWN_DELCIRCULARAREA);
        } else if (arg == "setrectanglearea") {
          retval = DealSetRectangleAreaRequest(*device_it, va_vec);
        } else if (arg == "delrectanglearea") {
          retval = DealDeleteAreaRouteRequest(*device_it, va_vec,
                                              DOWN_DELRECTANGLEAREA);
        } else if (arg == "setpolygonalarea") {
          retval = DealSetPolygonalAreaRequest(*device_it, va_vec);
        } else if (arg == "delpolygonalarea") {
          retval = DealDeleteAreaRouteRequest(*device_it, va_vec,
                                              DOWN_DELPOLYGONALAREA);
        } else if (arg == "setroute") {
          retval = DealSetRouteRequest(*device_it, va_vec);
        } else if (arg == "delroute") {
          retval = DealDeleteAreaRouteRequest(*device_it, va_vec,
                                              DOWN_DELROUTE);
        } else if (arg == "getpositioninfo") {
          retval = DealGetPositionInfoRequest(*device_it);
        } else if (arg == "positiontrack") {
          retval = DealPositionTrackRequest(*device_it, va_vec);
        } else if (arg == "terminalcontrol") {
          retval = DealTerminalControlRequest(*device_it, va_vec);
        } else if (arg == "vehiclecontrol") {
          retval = DealVehicleControlRequest(*device_it, va_vec);
        }
        if (retval == 0) {
          memcpy(buffer, "operation completed.", 20);
        } else {
          retval = 0;
          memcpy(buffer, "operation failed!!!", 19);
        }
        EpollRegister(epoll_fd_, device_it->socket_fd);
      }
    } else if (device_it != device_list_.end()) {
      memcpy(buffer, "device has not connect!!!\n", 25);
    } else {
      memcpy(buffer, "has not such device!!!\n", 22);
    }
  }

  va_vec.clear();
  return retval;
}

bool Jt808Service::CheckPacketComplete(int &sock, ProtocolParameters &propara) {
  Message msg = {0};
  if (propara.packet_total_num &&
      (propara.packet_total_num ==
       propara.packet_response_success_num)) {
    return true;
  } else {
    while (1) {
      if (RecvFrameData(sock, msg)) {
        close(sock);
        sock = -1;
        break;
      } else if (msg.size > 0) {
        if (Jt808FrameParse(msg, propara) == DOWN_PACKETRESEND) {
          memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
          msg.size = Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
          if (SendFrameData(sock, msg)) {
            close(sock);
            sock = -1;
          }
          break;
        }
      }
      if (sock == -1) break;
      auto packet_id_it = propara.packet_id_list->begin();
      for (auto packet : *propara.packet_map) {
        if (packet_id_it == propara.packet_id_list->end()) break;
        if (packet.first == *packet_id_it) {
          if (SendFrameData(sock, packet.second)) {
            close(sock);
            sock = -1;
            break;
          } else {
            while (1) {
              if (RecvFrameData(sock, msg)) {
                close(sock);
                sock = -1;
                break;
              } else if (msg.size > 0) {
                if ((Jt808FrameParse(msg, propara) == UP_UNIRESPONSE) &&
                    (propara.respond_id == DOWN_UPDATEPACKAGE)) {
                  break;
                }
              }
            }
            if (sock == -1) break;
          }
          packet_id_it = propara.packet_id_list->erase(packet_id_it);
        }
      }
      if (sock == -1) break;
    }
  }
  return false;
}

void Jt808Service::UpgradeHandler(void) {
  Message msg;
  ProtocolParameters propara;
  std::ifstream ifs;
  char *data = nullptr;
  uint32_t len;
  uint32_t max_data_len;

  if (!device_list_.empty()) {
    for (auto &device : device_list_) {
      if (device.has_upgrade) {
        device.has_upgrade = false;
        memset(&propara, 0x0, sizeof(propara));
        memcpy(propara.version_num, device.upgrade_version,
               strlen(device.upgrade_version));
        max_data_len = 1023 - 11 - strlen(device.upgrade_version);
        EpollUnregister(epoll_fd_, device.socket_fd);
        ifs.open(device.file_path, std::ios::binary | std::ios::in);
        if (ifs.is_open()) {
          ifs.seekg(0, std::ios::end);
          len = ifs.tellg();
          ifs.seekg(0, std::ios::beg);
          data = new char[len];
          ifs.read(data, len);
          ifs.close();

          propara.packet_total_num = len/max_data_len + 1;
          propara.packet_sequence_num = 1;
          propara.upgrade_type = device.upgrade_type;
          propara.version_num_len = strlen(device.upgrade_version);
          propara.packet_map = new std::map<uint16_t, Message>;
          propara.packet_id_list = new std::list<uint16_t>;
          while (len > 0) {
            memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
            if (len > max_data_len) {
              propara.packet_data_len = max_data_len;
            } else {
              propara.packet_data_len = len;
            }
            len -= propara.packet_data_len;
            // prepare data content of the upgrade file.
            memset(propara.packet_data, 0x0, sizeof(propara.packet_data));
            memcpy(propara.packet_data,
                   data + max_data_len * (propara.packet_sequence_num - 1),
                   propara.packet_data_len);
            msg.size = Jt808FramePack(msg, DOWN_UPDATEPACKAGE, propara);
            propara.packet_map->insert(
                std::make_pair(propara.packet_sequence_num, msg));
            if (SendFrameData(device.socket_fd, msg)) {
              close(device.socket_fd);
              device.socket_fd = -1;
              break;
            } else {
              while (1) {
                if (RecvFrameData(device.socket_fd, msg)) {
                  close(device.socket_fd);
                  device.socket_fd = -1;
                  break;
                } else if (msg.size > 0) {
                  if ((Jt808FrameParse(msg, propara) == UP_UNIRESPONSE) &&
                      (propara.respond_id == DOWN_UPDATEPACKAGE)) {
                    break;
                  }
                }
              }
              if (device.socket_fd == -1) {
                break;
              }
              ++propara.packet_sequence_num;
              usleep(1000);
            }
          }
          while (!CheckPacketComplete(device.socket_fd, propara)) {
            if (device.socket_fd < 0) break;
          }
          if (device.socket_fd > 0) {
            EpollRegister(epoll_fd_, device.socket_fd);
          }
          propara.packet_id_list->clear();
          propara.packet_map->clear();
          delete propara.packet_id_list;
          delete propara.packet_map;
          delete [] data;
          data = nullptr;
        }
        break;
      }
    }
  }
}

