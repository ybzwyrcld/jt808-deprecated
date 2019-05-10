#include "service/jt808_service.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <thread>

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "bcd/bcd.h"
#include "unix_socket/unix_socket.h"


using std::ifstream;
using std::string;
using std::stringstream;

static int EpollRegister(const int &epoll_fd, const int &fd) {
  struct epoll_event ev;
  int ret, flags;

  /* important: make the fd non-blocking */
  flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  ev.events = EPOLLIN;
  //ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = fd;
  do {
      ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

static int EpollUnregister(const int &epoll_fd, const int &fd) {
  int ret;

  do {
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
  } while (ret < 0 && errno == EINTR);

  return ret;
}

static inline uint16_t EndianSwap16(const uint16_t &value) {
  assert(sizeof(value) == 2);

  return ((value & 0xff) << 8) | (value >> 8);
}

static inline uint32_t EndianSwap32(const uint32_t &value) {
  assert(sizeof(value) == 4);

  return (value >> 24)
      | ((value & 0x00ff0000) >> 8)
      | ((value & 0x0000ff00) << 8)
      | (value << 24);
}

static uint8_t BccCheckSum(const uint8_t *src, const int &len) {
    int16_t i = 0;
  uint8_t checksum = 0;

  for (i = 0; i < len; ++i) {
    checksum = checksum ^ src[i];
  }

  return checksum;
}

static uint16_t Escape(uint8_t *data, const int &len) {
  uint16_t i, j;

  uint8_t *buf = new uint8_t[len * 2];
  memset(buf, 0x0, len * 2);

  for (i = 0, j = 0; i < len; ++i) {
    if (data[i] == PROTOCOL_SIGN) {
      buf[j++] = PROTOCOL_ESCAPE;
      buf[j++] = PROTOCOL_ESCAPE_SIGN;
    } else if(data[i] == PROTOCOL_ESCAPE) {
      buf[j++] = PROTOCOL_ESCAPE;
      buf[j++] = PROTOCOL_ESCAPE_ESCAPE;
    } else {
      buf[j++] = data[i];
    }
  }

  memcpy(data, buf, j);
  delete [] buf;
  return j;
}

static uint16_t UnEscape(uint8_t *data, const int &len) {
  uint16_t i, j;

  uint8_t *buf = new uint8_t[len];
  memset(buf, 0x0, len);

  for (i = 0, j = 0; i < len; ++i) {
    if ((data[i] == PROTOCOL_ESCAPE) && (data[i+1] == PROTOCOL_ESCAPE_SIGN)) {
      buf[j++] = PROTOCOL_SIGN;
      ++i;
    } else if ((data[i] == PROTOCOL_ESCAPE) &&
               (data[i+1] == PROTOCOL_ESCAPE_ESCAPE)) {
      buf[j++] = PROTOCOL_ESCAPE;
      ++i;
    } else {
      buf[j++] = data[i];
    }
  }

  memcpy(data, buf, j);
  delete [] buf;
  return j;
}

static inline void PreparePhoneNum(const char *src, uint8_t *bcd_array) {
  char phone_num[6] = {0};

  BcdFromStringCompress(src, phone_num, strlen(src));
  memcpy(bcd_array, phone_num, 6);
}

static uint8_t GetParameterTypeByParameterId(const uint32_t &para_id) {
  switch (para_id) {
    case GNSSPOSITIONMODE: case GNSSBAUDERATE: case GNSSOUTPUTFREQ:
    case GNSSUPLOADWAY: case STARTUPGPS: case STARTUPCDRADIO:
    case STARTUPNTRIPCORS: case STARTUPNTRIPSERV: case STARTUPJT808SERV:
    case GPSLOGGGA: case GPSLOGRMC: case GPSLOGATT:
    case CDRADIORECEIVEMODE: case CDRADIOFORMCODE: case NTRIPCORSREPORTINTERVAL:
    case NTRIPSERVICEREPORTINTERVAL: case JT808SERVICEREPORTINTERVAL:
      return kByteType;
    case CAN1UPLOADINTERVAL: case CAN2UPLOADINTERVAL: case CDRADIOWORKINGFREQ:
    case NTRIPCORSPORT: case NTRIPSERVICEPORT: case JT808SERVICEPORT:
      return kWordType;
    case HEARTBEATINTERVAL: case TCPRESPONDTIMEOUT: case TCPMSGRETRANSTIMES:
    case UDPRESPONDTIMEOUT: case UDPMSGRETRANSTIMES: case SMSRESPONDTIMEOUT:
    case SMSMSGRETRANSTIMES: case POSITIONREPORTWAY: case POSITIONREPORTPLAN:
    case NOTLOGINREPORTTIMEINTERVAL: case SLEEPREPORTTIMEINTERVAL:
    case ALARMREPORTTIMEINTERVAL: case DEFTIMEREPORTTIMEINTERVAL:
    case NOTLOGINREPORTDISTANCEINTERVAL: case SLEEPREPORTDISTANCEINTERVAL:
    case ALARMREPORTDISTANCEINTERVAL: case DEFTIMEREPORTDISTANCEINTERVAL:
    case INFLECTIONPOINTRETRANSANGLE: case ALARMSHIELDWORD: case ALARMSENDTXT:
    case ALARMSHOOTSWITCH: case ALARMSHOOTSAVEFLAGS: case ALARMKEYFLAGS:
    case MAXSPEED: case GNSSOUTPUTCOLLECTFREQ: case GNSSUPLOADSET:
    case CAN1COLLECTINTERVAL: case CAN2COLLECTINTERVAL: case CDRADIOBAUDERATE:
      return kDwordType;
    case CANSPECIALSET: case NTRIPCORSIP: case NTRIPCORSUSERNAME:
    case NTRIPCORSPASSWD: case NTRIPCORSMOUNTPOINT: case NTRIPSERVICEIP:
    case NTRIPSERVICEUSERNAME: case NTRIPSERVICEPASSWD:
    case NTRIPSERVICEMOUNTPOINT: case JT808SERVICEIP: case JT808SERVICEPHONENUM:
      return kStringType;
    default:
      return kUnknowType;
   }
}

static uint8_t GetParameterLengthByParameterType(const uint8_t &para_type) {
  switch (para_type) {
    case kByteType:
      return 1;
    case kWordType:
      return 2;
    case kDwordType:
      return 4;
    case kStringType:
    case kUnknowType:
    default:
      return 0;
   }
}

// Generate a node based on the parameter id and add it to the parameter list.
void AddParameterNodeByParameterId(const uint32_t &para_id,
                                   list<TerminalParameter *> *para_list,
                                   const char *para_value) {
  if (para_list == nullptr)
    return ;

  TerminalParameter *node = new TerminalParameter;
  memset(node, 0x0, sizeof(*node));
  node->parameter_id = para_id;
  node->parameter_type = GetParameterTypeByParameterId(para_id);
  node->parameter_len = GetParameterLengthByParameterType(
                             node->parameter_type);
  if (para_value != nullptr) {
    if (node->parameter_type == kStringType) {
      node->parameter_len = strlen(para_value);
    }
    memcpy(node->parameter_value, para_value, node->parameter_len);
  }
  para_list->push_back(node);
}

template <class T>
static void ClearListElement(list<T *> &list) {
  if (list.empty())
    return ;

  auto element_it = list.begin();
  while (element_it != list.end()) {
    delete (*element_it);
    element_it = list.erase(element_it);
  }
}

template <class T>
static void ClearListElement(list<T *> *list) {
  if (list == nullptr || list->empty())
    return ;

  auto element_it = list->begin();
  while (element_it != list->end()) {
    delete (*element_it);
    element_it = list->erase(element_it);
  }

  delete list;
  list = nullptr;
}

static void ReadDevicesList(const char *path, list<DeviceNode> &list) {
  char *result;
  char line[512] = {0};
  char flags = ';';
  uint32_t u32val;
  ifstream ifs;

  ifs.open(path, std::ios::in | std::ios::binary);
  if (ifs.is_open()) {
    string str;
    while (getline(ifs, str)) {
      DeviceNode node = {0};
      memset(line, 0x0, sizeof(line));
      str.copy(line, str.length(), 0);
      result = strtok(line, &flags);
      str = result;
      str.copy(node.phone_num, str.size(), 0);
      result = strtok(NULL, &flags);
      str = result;
      u32val = 0;
      sscanf(str.c_str(), "%u", &u32val);
      memcpy(node.authen_code, &u32val, 4);
      node.socket_fd = -1;
      list.push_back(node);
    }
    ifs.close();
  }
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
  if (epoll_events_ == NULL){
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
  if (epoll_events_ == NULL){
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
  MessageData msg = {0};

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

  if (!RecvFrameData(new_sock, message_)) {
    memset(&propara, 0x0, sizeof(propara));
    command = Jt808FrameParse(message_, propara);
    switch (command) {
      case UP_REGISTER:
        memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        msg.len = Jt808FramePack(msg, DOWN_REGISTERRSPONSE, propara);
        SendFrameData(new_sock, msg);
        if (propara.respond_result != 0x0) {
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
        message_.len = Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
        SendFrameData(new_sock, msg);
        if (propara.respond_result != 0x0) {
          close(new_sock);
          new_sock = -1;
        } else if (!device_list_.empty()) {
          device_it = device_list_.begin();
          while (device_it != device_list_.end()) {
            BcdFromStringCompress(device_it->phone_num,
                                  phone_num, strlen(device_it->phone_num));
            if (memcmp(phone_num, propara.phone_num, 6) == 0)
              break;
            else
              ++device_it;
          }
          if (device_it != device_list_.end()) {
            memcpy(device_it->manufacturer_id, propara.manufacturer_id, 5);
            device_it->socket_fd = new_sock;
          }
        }
        break;
      default:
        close(new_sock);
        new_sock = -1;
        break;
    }
  }

  if (new_sock > 0)
    EpollRegister(epoll_fd_, new_sock);

  return new_sock;
}

void Jt808Service::Run(const int &time_out) {
  int ret = -1;
  int i;
  int active_count;
  char recv_buff[65536] = {0};
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  while(1){
    ret = Jt808ServiceWait(time_out);
    if(ret == 0){
      //printf("time out\n");
      continue;
    } else {
      active_count = ret;
      for (i = 0; i < active_count; i++) {
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
            if (ParseCommand(recv_buff) >= 0)
               send(client_fd_, recv_buff, strlen(recv_buff), 0);
          }
          close(client_fd_);
        } else if ((epoll_events_[i].events & EPOLLIN) &&
                   !device_list_.empty()) {
          auto device_it = device_list_.begin();
          while (device_it != device_list_.end()) {
            if (epoll_events_[i].data.fd == device_it->socket_fd) {
              if (!RecvFrameData(epoll_events_[i].data.fd, msg)) {
                Jt808FrameParse(msg, propara);
              } else {
                EpollUnregister(epoll_fd_, epoll_events_[i].data.fd);
                close(epoll_events_[i].data.fd);
                device_it->socket_fd = -1;
              }
              break;
            } else {
              ++device_it;
            }
          }
        } else {
        }
      }
    }
  }
}

int Jt808Service::SendFrameData(const int &fd, const MessageData &msg) {
  int ret = -1;

  signal(SIGPIPE, SIG_IGN);
  ret = send(fd, msg.buffer, msg.len, 0);
  if (ret < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      //printf("%s[%d]: no data to send, continue\n", __FILE__, __LINE__);
      ret = 0;
    } else {
      if (errno == EPIPE) {
        printf("%s[%d]: remote socket close!!!\n", __FILE__, __LINE__);
      }
      ret = -1;
      printf("%s[%d]: send data failed!!!\n", __FILE__, __LINE__);
    }
  } else if (ret == 0) {
    printf("%s[%d]: connection disconect!!!\n", __FILE__, __LINE__);
    ret = -1;
  }

  return ret >= 0 ? 0 : ret;
}

int Jt808Service::RecvFrameData(const int &fd, MessageData &msg) {
  int ret = -1;

  memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
  ret = recv(fd, msg.buffer, MAX_PROFRAMEBUF_LEN, 0);
  if (ret < 0) {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
      //printf("%s[%d]: no data to receive, continue\n", __FILE__, __LINE__);
      ret = 0;
    } else {
      ret = -1;
      printf("%s[%d]: recv data failed!!!\n", __FILE__, __LINE__);
    }
    msg.len = 0;
  } else if(ret == 0) {
    ret = -1;
    msg.len = 0;
    printf("%s[%d]: connection disconect!!!\n", __FILE__, __LINE__);
  } else {
    msg.len = ret;
  }

  return ret >= 0 ? 0 : ret;
}

int Jt808Service::Jt808FramePack(MessageData &msg,
                                 const uint16_t &command,
                                 const ProtocolParameters &propara) {
  uint8_t *msg_body;
  uint16_t u16temp;
  uint32_t u32temp;
  MessageHead *msghead_ptr;

  msghead_ptr = (MessageHead *)&msg.buffer[1];
  msghead_ptr->id = EndianSwap16(command);
  msghead_ptr->attribute.val = 0x0000;
  msghead_ptr->attribute.bit.encrypt = 0;
  if (propara.packet_total_num > 1) {  // need to divide the package.
    msghead_ptr->attribute.bit.package = 1;
    msg_body = &msg.buffer[MSGBODY_PACKAGE_POS];
    // packet total num.
    u16temp = propara.packet_total_num;
    u16temp = EndianSwap16(u16temp);
    memcpy(&msg.buffer[13], &u16temp, 2);
    // packet sequence num.
    u16temp = propara.packet_sequence_num;
    u16temp = EndianSwap16(u16temp);
    memcpy(&msg.buffer[15], &u16temp, 2);
    msg.len = 16;
  } else {
    msghead_ptr->attribute.bit.package = 0;
    msg_body = &msg.buffer[MSGBODY_NOPACKAGE_POS];
    msg.len = 12;
  }

  message_flow_num_++;
  msghead_ptr->msgflownum = EndianSwap16(message_flow_num_);
  memcpy(msghead_ptr->phone, propara.phone_num, 6);

  switch (command) {
    case DOWN_UNIRESPONSE:
      // terminal message flow num.
      u16temp = EndianSwap16(propara.respond_flow_num);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // terminal message id.
      u16temp = EndianSwap16(propara.respond_id);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // result.
      *msg_body = propara.respond_result;
      msg_body++;
      msghead_ptr->attribute.bit.msglen = 5;
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      msg.len += 5;
      break;
    case DOWN_REGISTERRSPONSE:
      // terminal message flow num.
      u16temp = EndianSwap16(propara.respond_flow_num);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // result.
      *msg_body = propara.respond_result;
      msg_body++;
      if (propara.respond_result == 0) {
        // send authen code if register success.
        memcpy(msg_body, propara.authen_code, 4);
        msg_body += 4;
        msghead_ptr->attribute.bit.msglen = 7;
        msg.len += 7;
      } else {
        msghead_ptr->attribute.bit.msglen = 3;
        msg.len += 3;
      }
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      break;
    case DOWN_SETTERMPARA:
      if ((propara.terminal_parameter_list != nullptr) &&
          !propara.terminal_parameter_list->empty()) {
        msg_body[0] = propara.terminal_parameter_list->size();
        msg_body++;
        msg.len++;
        msghead_ptr->attribute.bit.msglen = 1;
        auto paralist_it = propara.terminal_parameter_list->begin();
        while (paralist_it != propara.terminal_parameter_list->end()) {
          u32temp = EndianSwap32((*paralist_it)->parameter_id);
          memcpy(msg_body, &u32temp, 4);
          msg_body += 4;
          memcpy(msg_body, &((*paralist_it)->parameter_len), 1);
          msg_body++;
          switch (GetParameterTypeByParameterId((*paralist_it)->parameter_id)) {
            case kWordType:
              memcpy(&u16temp, (*paralist_it)->parameter_value, 2);
              u16temp = EndianSwap16(u16temp);
              memcpy(msg_body, &u16temp, 2);
              break;
            case kDwordType:
              memcpy(&u32temp, (*paralist_it)->parameter_value, 4);
              u32temp = EndianSwap32(u32temp);
              memcpy(msg_body, &u32temp, 4);
              break;
            case kByteType:
            case kStringType:
              memcpy(msg_body, (*paralist_it)->parameter_value,
                     (*paralist_it)->parameter_len);
              break;
            default:
              break;
          }
          msg_body += (*paralist_it)->parameter_len;
          msg.len += 5+(*paralist_it)->parameter_len;
          msghead_ptr->attribute.bit.msglen += 5+(*paralist_it)->parameter_len;
          ++paralist_it;
        }
      } else {
        msg_body[0] = 0;
        msg_body++;
        msg.len++;
        msghead_ptr->attribute.bit.msglen = 1;
      }
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      break;
    case DOWN_GETTERMPARA:
      break;
    case DOWN_GETSPECTERMPARA:
      // terminal parameters total num.
      *msg_body = propara.terminal_parameter_list->size();
      msg_body++;
      msg.len++;
      msghead_ptr->attribute.bit.msglen = 1;
      // terminal parameters id.
      if ((propara.terminal_parameter_list != nullptr) &&
          !propara.terminal_parameter_list->empty()) {
        auto para_it = propara.terminal_parameter_list->begin();
        while(para_it != propara.terminal_parameter_list->end()) {
          u32temp = (*para_it)->parameter_id;
          u32temp = EndianSwap32(u32temp);
          memcpy(msg_body, &u32temp, 4);
          msg_body += 4;
          msg.len += 4;
          msghead_ptr->attribute.bit.msglen += 4;
          ++para_it;
        }
      }
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      break;
    case DOWN_UPDATEPACKAGE:
      // upgrade type.
      *msg_body = propara.upgrade_type;
      msg_body++;
      memcpy(msg_body, propara.manufacturer_id, 5);
      msg_body += 5;
      msg.len += 6;
      // length of upgrade version name.
      *msg_body = propara.version_num_len;
      msg_body++;
      msg.len++;
      // upgrade version name.
      memcpy(msg_body, propara.version_num, propara.version_num_len);
      msg_body += propara.version_num_len;
      msg.len += propara.version_num_len;
      // length of valid data content.
      u32temp = EndianSwap32(propara.packet_data_len);
      memcpy(msg_body, &u32temp, 4);
      msg_body += 4;
      msg.len += 4;
      // valid content of the upgrade file.
      memcpy(msg_body, propara.packet_data, propara.packet_data_len);
      msg_body += propara.packet_data_len;
      msg.len += propara.packet_data_len;
      msghead_ptr->attribute.bit.msglen = 11 + propara.version_num_len +
                                          propara.packet_data_len;
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      break;
    default:
      break;
  }

  *msg_body = BccCheckSum(&msg.buffer[1], msg.len);
  msg.len++;

  msg.len = Escape(msg.buffer + 1, msg.len);
  msg.buffer[0] = PROTOCOL_SIGN;
  msg.len++;
  msg.buffer[msg.len++] = PROTOCOL_SIGN;

  // printf("%s[%d]: socket-send:\n", __FILE__, __LINE__);
  // for (uint16_t i = 0; i < msg.len; ++i) {
  //   printf("%02X ", msg.buffer[i]);
  // }
  // printf("\r\n");

  return msg.len;
}

uint16_t Jt808Service::Jt808FrameParse(MessageData &msg,
                                       ProtocolParameters &propara) {
  uint8_t *msg_body;
  char phone_num[12] = {0};
  uint8_t u8temp;
  uint16_t msglen;
  uint16_t retval;
  uint16_t u16temp = 0;
  uint32_t u32temp;
  double latitude;
  double longitude;
  float altitude;
  float speed;
  float bearing;
  char timestamp[6];
  MessageHead *msghead_ptr;

  // printf("%s[%d]: socket-recv:\n", __FILE__, __LINE__);
  // for (uint16_t i = 0; i < msg.len; ++i) {
  //   printf("%02X ", msg.buffer[i]);
  // }
  // printf("\r\n");

  msg.len = UnEscape(&msg.buffer[1], msg.len);
  msghead_ptr = (MessageHead *)&msg.buffer[1];
  msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);

  if (msghead_ptr->attribute.bit.package) {  // the flags of divide the package.
    msg_body = &msg.buffer[MSGBODY_PACKAGE_POS];
  } else {
    msg_body = &msg.buffer[MSGBODY_NOPACKAGE_POS];
  }

  msghead_ptr->msgflownum = EndianSwap16(msghead_ptr->msgflownum);
  u16temp = EndianSwap16(msghead_ptr->id);
  retval = u16temp;
  msglen = static_cast<uint16_t>(msghead_ptr->attribute.bit.msglen);
  switch (u16temp) {
    case UP_UNIRESPONSE:
      // message id.
      u16temp = 0;
      memcpy(&u16temp, &msg_body[2], 2);
      propara.respond_id = EndianSwap16(u16temp);
      switch(propara.respond_id) {
        case DOWN_UPDATEPACKAGE:
          printf("%s[%d]: received updatepackage respond: ",
                 __FILE__, __LINE__);
          break;
        case DOWN_SETTERMPARA:
          printf("%s[%d]: set terminal parameter respond: ",
                 __FILE__, __LINE__);
          break;
        default:
          break;
      }
      // result.
      if (msg_body[4] == 0x00) {
        printf("normal\r\n");
      } else if(msg_body[4] == 0x01) {
        printf("failed\r\n");
      } else if(msg_body[4] == 0x02) {
        printf("message has something wrong\r\n");
      } else if(msg_body[4] == 0x03) {
        printf("message not support\r\n");
      }
      break;
    case UP_REGISTER:
      memset(&propara, 0x0, sizeof(propara));
      propara.respond_flow_num = EndianSwap16(msghead_ptr->msgflownum);
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      // check phone num.
      if (!device_list_.empty()) {
        auto device_it = device_list_.begin();
        while (device_it != device_list_.end()) {
          BcdFromStringCompress(device_it->phone_num,
                                phone_num,
                                strlen(device_it->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++device_it;
        }
        if (device_it != device_list_.end()) { // find phone num.
          // make sure there is no device connection.
          if (device_it->socket_fd == -1) {
            propara.respond_result = 0;
            memcpy(propara.authen_code, device_it->authen_code, 4);
            memcpy(propara.manufacturer_id, &msg_body[4], 5);
          } else {
            propara.respond_result = 3;
          }
        } else {
          propara.respond_result = 4;
        }
      } else {
        propara.respond_result = 2;
      }
      break;
    case UP_AUTHENTICATION:
      memset(&propara, 0x0, sizeof(propara));
      propara.respond_flow_num = EndianSwap16(msghead_ptr->msgflownum);
      propara.respond_id = EndianSwap16(msghead_ptr->id);
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      if (!device_list_.empty()) {
        auto device_it = device_list_.begin();
        while (device_it != device_list_.end()) {
          BcdFromStringCompress(device_it->phone_num,
                                phone_num,
                                strlen(device_it->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++device_it;
        }
        if ((device_it != device_list_.end()) &&
            (memcmp(device_it->authen_code, msg_body, msglen) == 0)) {
          propara.respond_result = 0;
        } else {
          propara.respond_result = 1;
        }
      } else {
        propara.respond_result = 1;
      }
      break;
    case UP_GETPARASPONSE:
      printf("%s[%d]: received get terminalparameter respond\n", __FILE__, __LINE__);
      propara.respond_flow_num = EndianSwap16(msghead_ptr->msgflownum);
      propara.respond_id = EndianSwap16(msghead_ptr->id);
      msg_body += 3;
      if ((propara.terminal_parameter_list != nullptr) &&
          !propara.terminal_parameter_list->empty()) {
        auto para_it = propara.terminal_parameter_list->begin();
        while (para_it != propara.terminal_parameter_list->end()) {
          memcpy(&u32temp, &msg_body[0], 4);
          u32temp = EndianSwap32(u32temp);
          if (u32temp != (*para_it)->parameter_id) {
            propara.respond_result = 1;
            break;
          }
          msg_body += 4;
          u8temp = msg_body[0];
          msg_body++;
          switch ((*para_it)->parameter_type) {
            case kWordType:
              memcpy(&u16temp, &msg_body[0], u8temp);
              u16temp = EndianSwap16(u16temp);
              memcpy((*para_it)->parameter_value, &u16temp, u8temp);
              break;
            case kDwordType:
              memcpy(&u32temp, &msg_body[0], u8temp);
              u32temp = EndianSwap32(u32temp);
              memcpy((*para_it)->parameter_value, &u32temp, u8temp);
              break;
            case kByteType:
            case kStringType:
              memcpy((*para_it)->parameter_value, &msg_body[0], u8temp);
              break;
            default:
              break;
          }
          msg_body += u8temp;
          ++para_it;
        }
        propara.respond_result = 0;
      } else if (propara.terminal_parameter_list->empty()) {
        int len = msglen - 3;
        char parameter_value[256] ={0};
        uint32_t parameter_id = 0;
        while (len) {
          memcpy(&u32temp, &msg_body[0], 4);
          parameter_id = EndianSwap32(u32temp);
          msg_body += 4;
          u8temp = msg_body[0];
          msg_body++;
          memset(parameter_value, 0x0, sizeof(parameter_value));
          switch (GetParameterTypeByParameterId(parameter_id)) {
            case kWordType:
              memcpy(&u16temp, &msg_body[0], u8temp);
              u16temp = EndianSwap16(u16temp);
              memcpy(parameter_value, &u16temp, u8temp);
              break;
            case kDwordType:
              memcpy(&u32temp, &msg_body[0], u8temp);
              u32temp = EndianSwap32(u32temp);
              memcpy(parameter_value, &u32temp, u8temp);
              break;
            case kByteType:
            case kStringType:
              memcpy(parameter_value, &msg_body[0], u8temp);
              break;
            default:
              break;
          }
          AddParameterNodeByParameterId(parameter_id,
                                        propara.terminal_parameter_list,
                                        parameter_value);
          msg_body += u8temp;
          len -= (u8temp + 5);
        }
        propara.respond_result = 0;
      }
      break;
    case UP_UPDATERESULT:
      printf("%s[%d]: received updateresult respond: ", __FILE__, __LINE__);
      if (msg_body[4] == 0x00) {
        printf("normal\r\n");
      } else if(msg_body[4] == 0x01) {
        printf("failed\r\n");
      } else if(msg_body[4] == 0x02) {
        printf("message has something wrong\r\n");
      } else if(msg_body[4] == 0x03) {
        printf("message not support\r\n");
      }
      break;
    case UP_POSITIONREPORT:
      printf("%s[%d]: received position report respond:\n", __FILE__, __LINE__);
      memset(phone_num, 0x0, sizeof(phone_num));
      StringFromBcdCompress(reinterpret_cast<char *>(msghead_ptr->phone),
                            phone_num, 6);
      printf("\tdevice: %s\n", phone_num);
      memcpy(&u32temp, &msg_body[12], 4);
      latitude = EndianSwap32(u32temp) / 1000000.0;
      printf("\tlatitude: %lf\n", latitude);
      memcpy(&u32temp, &msg_body[8], 4);
      longitude = EndianSwap32(u32temp) / 1000000.0;
      printf("\tlongitude: %lf\n", longitude);
      memcpy(&u16temp, &msg_body[16], 2);
      altitude = EndianSwap16(u16temp);
      printf("\taltitude: %f\n", altitude);
      memcpy(&u16temp, &msg_body[18], 2);
      speed = EndianSwap16(u16temp) / 10.0;
      printf("\tspeed: %f\n", speed);
      memcpy(&u16temp, &msg_body[20], 2);
      bearing = EndianSwap16(u16temp);
      printf("\tbearing: %f\n", bearing);
      timestamp[0] = HexFromBcd(msg_body[22]);
      timestamp[1] = HexFromBcd(msg_body[23]);
      timestamp[2] = HexFromBcd(msg_body[24]);
      timestamp[3] = HexFromBcd(msg_body[25]);
      timestamp[4] = HexFromBcd(msg_body[26]);
      timestamp[5] = HexFromBcd(msg_body[27]);
      printf("\ttimestamp: 20%02d-%02d-%02d, %02d:%02d:%02d\n",
             timestamp[0], timestamp[1], timestamp[2],
             timestamp[3], timestamp[4], timestamp[5]);
      printf("%s[%d]: received position report respond end\n",
             __FILE__, __LINE__);
    default:
      break;
  }

  return retval;
}

int Jt808Service::DealGetStartupRequest(DeviceNode &device, char *result) {
  int retval = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(STARTUPGPS,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(STARTUPCDRADIO,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(STARTUPNTRIPCORS,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(STARTUPNTRIPSERV,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(STARTUPJT808SERV,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto para_it = propara.terminal_parameter_list->begin();
        uint8_t value = 0;
        string str = "startup info:";
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [gps]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [cdradio]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [ntrip cors]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [ntrip servcie]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [jt808 service]" : "";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetStartupRequest(DeviceNode &device,
                                        vector<string> &va_vec) {
  int retval = 0;
  char value[5] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  if (arg == "gps") {
    value[0] = 1;
    AddParameterNodeByParameterId(STARTUPGPS,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(STARTUPGPS,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "cdradio") {
    value[0] = 1;
    AddParameterNodeByParameterId(STARTUPCDRADIO,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(STARTUPCDRADIO,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "ntripcors") {
    value[0] = 1;
    AddParameterNodeByParameterId(STARTUPNTRIPCORS,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(STARTUPNTRIPCORS,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "ntripservice") {
    value[0] = 1;
    AddParameterNodeByParameterId(STARTUPNTRIPSERV,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(STARTUPNTRIPSERV,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "jt808service") {
    value[0] = 1;
    AddParameterNodeByParameterId(STARTUPJT808SERV,
                                  propara.terminal_parameter_list, value);
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(STARTUPJT808SERV,
                                  propara.terminal_parameter_list, value);
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetGpsRequest(DeviceNode &device, char *result) {
  int retval = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(GPSLOGGGA,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(GPSLOGRMC,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(GPSLOGATT,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto para_it = propara.terminal_parameter_list->begin();
        uint8_t value = 0;
        string str = "gps info:";
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [LOGGGA]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [LOGRMC]" : "";
        ++para_it;
        value = (*para_it)->parameter_value[0];
        str += value == 1 ? " [LOGATT]" : "";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetGpsRequest(DeviceNode &device,
                                    vector<string> &va_vec) {
  int retval = 0;
  char value[5] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  if (arg == "LOGGGA") {
    value[0] = 1;
    AddParameterNodeByParameterId(GPSLOGGGA,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(GPSLOGGGA,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "LOGRMC") {
    value[0] = 1;
    AddParameterNodeByParameterId(GPSLOGRMC,
                                  propara.terminal_parameter_list, value);
    va_vec.pop_back();
    if (!va_vec.empty())
      arg = va_vec.back();
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(GPSLOGRMC,
                                  propara.terminal_parameter_list, value);
  }
  if (arg == "LOGATT") {
    value[0] = 1;
    AddParameterNodeByParameterId(GPSLOGATT,
                                  propara.terminal_parameter_list, value);
  } else {
    value[0] = 0;
    AddParameterNodeByParameterId(GPSLOGATT,
                                  propara.terminal_parameter_list, value);
  }

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetCdradioRequest(DeviceNode &device, char *result) {
  int retval = 0;
  uint32_t u32val = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(CDRADIOBAUDERATE,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(CDRADIOWORKINGFREQ,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(CDRADIORECEIVEMODE,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(CDRADIOFORMCODE,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(CDRADIOFORMCODE,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto para_it = propara.terminal_parameter_list->begin();
        string str = "cdradio info: bauderate[";
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "],workfreqpoint[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "],recvmode[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str +="],formcode[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "]";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetCdradioRequest(DeviceNode &device,
                                        vector<string> &va_vec) {
  int retval = 0;
  uint32_t u32val = 0;
  char value[256] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  if (va_vec.size() != 4)
    return -1;

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u32val, 4);
  AddParameterNodeByParameterId(CDRADIOBAUDERATE,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint16_t u16val = static_cast<uint16_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u16val, 2);
  AddParameterNodeByParameterId(CDRADIOWORKINGFREQ,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint8_t u8val = static_cast<uint8_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u8val, 1);
  AddParameterNodeByParameterId(CDRADIORECEIVEMODE,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  u8val = static_cast<uint8_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u8val, 1);
  AddParameterNodeByParameterId(CDRADIOFORMCODE,
                                propara.terminal_parameter_list, value);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetNtripCorsRequest(DeviceNode &device, char *result) {
  int retval = 0;
  uint32_t u32val = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(NTRIPCORSIP,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPCORSPORT,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPCORSUSERNAME,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPCORSPASSWD,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPCORSMOUNTPOINT,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPCORSREPORTINTERVAL,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        string str = "ntrip cors info: ip[";
        auto para_it = propara.terminal_parameter_list->begin();
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str += "],port[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "],username[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],password[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],mountpoint[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],reportinterval[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "]";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetNtripCorsRequest(DeviceNode &device,
                                          vector<string> &va_vec) {
  int retval = 0;
  uint32_t u32val = 0;
  char value[5] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  if (va_vec.size() != 6)
    return -1;

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPCORSIP,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint16_t u16val = static_cast<uint16_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u16val, 2);
  AddParameterNodeByParameterId(NTRIPCORSPORT,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPCORSUSERNAME,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPCORSPASSWD,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPCORSMOUNTPOINT,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint8_t u8val = static_cast<uint8_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u8val, 1);
  AddParameterNodeByParameterId(NTRIPCORSREPORTINTERVAL,
                                propara.terminal_parameter_list, value);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetNtripServiceRequest(DeviceNode &device, char *result) {
  int retval = 0;
  uint32_t u32val = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(NTRIPSERVICEIP,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPSERVICEPORT,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPSERVICEUSERNAME,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPSERVICEPASSWD,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPSERVICEMOUNTPOINT,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(NTRIPSERVICEREPORTINTERVAL,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        string str = "ntrip service info: ip[";
        auto para_it = propara.terminal_parameter_list->begin();
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str += "],port[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "],username[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],password[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],mountpoint[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],reportinterval[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "]";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetNtripServiceRequest(DeviceNode &device,
                                             vector<string> &va_vec) {
  int retval = 0;
  uint32_t u32val = 0;
  char value[5] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  if (va_vec.size() != 6)
    return -1;

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPSERVICEIP,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint16_t u16val = static_cast<uint16_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u16val, 2);
  AddParameterNodeByParameterId(NTRIPSERVICEPORT,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPSERVICEUSERNAME,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPSERVICEPASSWD,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(NTRIPSERVICEMOUNTPOINT,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint8_t u8val = static_cast<uint8_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u8val, 1);
  AddParameterNodeByParameterId(NTRIPSERVICEREPORTINTERVAL,
                                propara.terminal_parameter_list, value);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetJt808ServiceRequest(DeviceNode &device, char *result) {
  int retval = 0;
  uint32_t u32val = 0;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  AddParameterNodeByParameterId(JT808SERVICEIP,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(JT808SERVICEPORT,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(JT808SERVICEPHONENUM,
                                propara.terminal_parameter_list, nullptr);
  AddParameterNodeByParameterId(JT808SERVICEREPORTINTERVAL,
                                propara.terminal_parameter_list, nullptr);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto para_it = propara.terminal_parameter_list->begin();
        string str = "jt808 service info: ip[";
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str += "],port[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "],phone[";
        ++para_it;
        str += reinterpret_cast<char *>((*para_it)->parameter_value);
        str +="],reportinterval[";
        ++para_it;
        u32val = 0;
        memcpy(&u32val, (*para_it)->parameter_value,
               (*para_it)->parameter_len);
        str += std::to_string(u32val);
        str += "]";
        str.copy(result, str.length(), 0);
        printf("%s\n", result);
        retval = str.length();
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetJt808ServiceRequest(DeviceNode &device,
                                             vector<string> &va_vec) {
  int retval = 0;
  uint32_t u32val = 0;
  char value[5] = {0};
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  if (va_vec.size() != 4)
    return -1;

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(JT808SERVICEIP,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint16_t u16val = static_cast<uint16_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u16val, 2);
  AddParameterNodeByParameterId(JT808SERVICEPORT,
                                propara.terminal_parameter_list, value);
  arg = va_vec.back();
  va_vec.pop_back();
  AddParameterNodeByParameterId(JT808SERVICEPHONENUM,
                                propara.terminal_parameter_list,
                                arg.c_str());
  arg = va_vec.back();
  va_vec.pop_back();
  sscanf(arg.c_str(), "%u", &u32val);
  uint8_t u8val = static_cast<uint8_t>(u32val);
  memset(value, 0x0, sizeof(value));
  memcpy(value, &u8val, 1);
  AddParameterNodeByParameterId(JT808SERVICEREPORTINTERVAL,
                                propara.terminal_parameter_list, value);

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealGetTerminalParameterRequest(DeviceNode &device,
                      vector<string> &va_vec, char *result) {
  int retval = 0;
  uint16_t u16val;
  uint32_t u32val;
  uint32_t parameter_id;
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  PreparePhoneNum(device.phone_num, propara.phone_num);
  if (va_vec.empty()) {
    Jt808FramePack(msg, DOWN_GETTERMPARA, propara);
  } else {
    while (!va_vec.empty()) {
      arg = va_vec.back();
      va_vec.pop_back();
      sscanf(arg.c_str(), "%x", &u32val);
      parameter_id = u32val;
      AddParameterNodeByParameterId(parameter_id,
                                    propara.terminal_parameter_list, nullptr);
    }
    Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  }
  if (SendFrameData(device.socket_fd, msg)) {
    close(device.socket_fd);
    device.socket_fd = -1;
    retval = -1;
  } else {
    while (1) {
      memset(&msg, 0x0, sizeof(msg));
      if (RecvFrameData(device.socket_fd, msg)) {
        close(device.socket_fd);
        device.socket_fd = -1;
        retval = -1;
        break;
      } else if (msg.len > 0) {
        if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
          string str = "terminalparameter: ";
          char parameter_s[256] = {0};
          auto para_it = propara.terminal_parameter_list->begin();
          while (para_it != propara.terminal_parameter_list->end()) {
            memset(parameter_s, 0x0, sizeof(parameter_s));
            u16val = GetParameterTypeByParameterId((*para_it)->parameter_id);
            if (u16val == kStringType) {
              snprintf(parameter_s, sizeof(parameter_s), "%04X:%s",
                       (*para_it)->parameter_id, (*para_it)->parameter_value);
            } else {
              u32val = 0;
              memcpy(&u32val, (*para_it)->parameter_value,
                     (*para_it)->parameter_len);
              snprintf(parameter_s, sizeof(parameter_s), "%04X:%u",
                       (*para_it)->parameter_id, u32val);
            }
            str += parameter_s;
            if (++para_it != propara.terminal_parameter_list->end()) {
              str += ",";
            }
          }
          memset(&msg, 0x0, sizeof(msg));
          Jt808FramePack(msg, DOWN_UNIRESPONSE, propara);
          SendFrameData(device.socket_fd, msg);
          str.copy(result, str.size(), 0);
          break;
        }
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::DealSetTerminalParameterRequest(DeviceNode &device,
                      vector<string> &va_vec) {
  int retval = 0;
  char parameter_value[256] = {0};
  char value[256] = {0};
  uint8_t u8val = 0;
  uint16_t u16val = 0;
  uint32_t u32val = 0;
  uint32_t parameter_id = 0;
  string arg;
  ProtocolParameters propara = {0};
  MessageData msg = {0};

  propara.terminal_parameter_list = new list<TerminalParameter *>;
  while (!va_vec.empty()) {
    arg = va_vec.back();
    va_vec.pop_back();
    memset(value, 0x0, sizeof(value));
    sscanf(arg.c_str(), "%x:%s", &u32val, value);
    parameter_id = u32val;
    memset(parameter_value, 0x0, sizeof(parameter_value));
    switch (GetParameterTypeByParameterId(parameter_id)) {
      case kByteType:
        u8val = atoi(value);
        memcpy(parameter_value, &u8val, 1);
        break;
      case kWordType:
        u16val = atoi(value);
        memcpy(parameter_value, &u16val, 2);
        break;
      case kDwordType:
        u32val = atoi(value);
        memcpy(parameter_value, &u32val, 4);
        break;
      case kStringType:
        memcpy(parameter_value, value, strlen(value));
        break;
      case kUnknowType:
        continue;
    }
    AddParameterNodeByParameterId(parameter_id,
                                  propara.terminal_parameter_list,
                                  parameter_value);
  }

  if (propara.terminal_parameter_list->empty())
    return retval;

  PreparePhoneNum(device.phone_num, propara.phone_num);
  Jt808FramePack(msg, DOWN_SETTERMPARA, propara);
  SendFrameData(device.socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device.socket_fd, msg)) {
      close(device.socket_fd);
      device.socket_fd = -1;
      retval = -1;
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) &&
          (propara.respond_id == DOWN_SETTERMPARA)) {
        break;
      }
    }
  }
  // remove terminal parameter list after use.
  ClearListElement(propara.terminal_parameter_list);

  return retval;
}

int Jt808Service::ParseCommand(char *buffer) {
  int retval = 0;
  string arg;
  string phone_num;
  stringstream sstr;
  vector<string> va_vec;

  sstr.clear();
  sstr << buffer;
  do {
    arg.clear();
    sstr >> arg;
    if (arg.empty())
      break;
    va_vec.push_back(arg);
  } while (1);
  sstr.str("");
  sstr.clear();
  memset(buffer, 0x0, strlen(buffer));
  reverse(va_vec.begin(), va_vec.end());
  // auto va_it = va_vec.begin();
  // while (va_it != va_vec.end()) {
  //   printf("%s\n", va_it->c_str());
  //   va_it;
  // }

  arg = va_vec.back();
  va_vec.pop_back();
  if (!device_list_.empty()) {
    auto device_it = device_list_.begin();
    while (device_it != device_list_.end()) {
      phone_num = device_it->phone_num;
      if (arg == phone_num) {
        break;
      } else {
        ++device_it;
      }
    }

    if (device_it != device_list_.end() && (device_it->socket_fd > 0)) {
      arg = va_vec.back();
      va_vec.pop_back();
      if (arg == "upgrade") {
        arg = va_vec.back();
        va_vec.pop_back();
        if ((arg == "device") || (arg == "gps") ||
            (arg == "system") || (arg == "cdradio")) {
          if (arg == "device")
            device_it->upgrade_type = 0x0;
          else if (arg == "gps")
            device_it->upgrade_type = 0x34;
          else if (arg == "cdradio")
            device_it->upgrade_type = 0x35;
          else if (arg == "system")
            device_it->upgrade_type = 0x36;
          else
            return -1;

          arg = va_vec.back();
          va_vec.pop_back();
          memset(device_it->upgrade_version, 0x0, sizeof(device_it->upgrade_version));
          arg.copy(device_it->upgrade_version, arg.length(), 0);

          arg = va_vec.back();
          va_vec.pop_back();
          memset(device_it->file_path, 0x0, sizeof(device_it->file_path));
          arg.copy(device_it->file_path, arg.length(), 0);
          device_it->has_upgrade = true;

          // start upgrade thread.
          std::thread start_upgrade_thread(StartUpgradeThread, this);
          start_upgrade_thread.detach();
          memcpy(buffer, "complete.", 10);
        }
      } else if (arg == "get") {
        EpollUnregister(epoll_fd_, device_it->socket_fd);
        // type.
        arg = va_vec.back();
        va_vec.pop_back();
        if (arg == "startup") {
          retval = DealGetStartupRequest(*device_it, buffer);
        } else if (arg == "gps") {
          retval = DealGetGpsRequest(*device_it, buffer);
        } else if (arg == "cdradio") {
          retval = DealGetCdradioRequest(*device_it, buffer);
        } else if (arg == "ntripcors") {
          retval = DealGetNtripCorsRequest(*device_it, buffer);
        } else if (arg == "ntripservice") {
          retval = DealGetNtripServiceRequest(*device_it, buffer);
        } else if (arg == "jt808service") {
          retval = DealGetJt808ServiceRequest(*device_it, buffer);
        } else if (arg == "terminalparameter") {
          retval = DealGetTerminalParameterRequest(*device_it, va_vec, buffer);
        }
        EpollRegister(epoll_fd_, device_it->socket_fd);
      } else if (arg == "set") {
        EpollUnregister(epoll_fd_, device_it->socket_fd);
        // type.
        arg = va_vec.back();
        va_vec.pop_back();
        if (arg == "startup") {
          retval = DealSetStartupRequest(*device_it, va_vec);
        } else if (arg == "gps") {
          retval = DealSetGpsRequest(*device_it, va_vec);
        } else if (arg == "cdradio") {
          retval = DealSetCdradioRequest(*device_it, va_vec);
        } else if (arg == "ntripcors") {
          retval = DealSetNtripCorsRequest(*device_it, va_vec);
        } else if (arg == "ntripservice") {
          retval = DealSetNtripServiceRequest(*device_it, va_vec);
        } else if (arg == "jt808service") {
          retval = DealSetJt808ServiceRequest(*device_it, va_vec);
        } else if (arg == "terminalparameter") {
          retval = DealSetTerminalParameterRequest(*device_it, va_vec);
        }
        if (retval == 0)
          memcpy(buffer, "complete.", 10);
        EpollRegister(epoll_fd_, device_it->socket_fd);
      }
    }
  }

  va_vec.clear();
  return retval;
}

void Jt808Service::UpgradeHandler(void) {
  MessageData msg;
  ProtocolParameters propara;
  ifstream ifs;
  char *data = nullptr;
  uint32_t len;
  uint32_t max_data_len;

  if (!device_list_.empty()) {
    auto device_it = device_list_.begin();
    while (device_it != device_list_.end()) {
      if (device_it->has_upgrade) {
        device_it->has_upgrade = false;
        break;
      } else {
        ++device_it;
      }
    }

    if (device_it == device_list_.end())
      return ;

    memset(&propara, 0x0, sizeof(propara));
    memcpy(propara.version_num,
           device_it->upgrade_version, strlen(device_it->upgrade_version));
    max_data_len = 1023 - 11 - strlen(device_it->upgrade_version);
    EpollUnregister(epoll_fd_, device_it->socket_fd);
    //printf("path: %s\n", file_path);
    ifs.open(device_it->file_path, std::ios::binary | std::ios::in);
    if (ifs.is_open()) {
      ifs.seekg(0, std::ios::end);
      len = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      data = new char[len];
      ifs.read(data, len);
      ifs.close();

      // packet total num.
      propara.packet_total_num = len/max_data_len + 1;
      // packet sequence num.
      propara.packet_sequence_num = 1;
      // upgrade type.
      propara.upgrade_type = device_it->upgrade_type;
      // upgrade version name len.
      propara.version_num_len = strlen(device_it->upgrade_version);
      // valid content of the upgrade file.
      while (len > 0) {
        memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        if (len > max_data_len)
          propara.packet_data_len = max_data_len; // length of valid content.
        else
          propara.packet_data_len = len;
        len -= propara.packet_data_len;
        //printf("sum packet = %d, sub packet = %d\n",
        //       propara.packet_total_num, propara.packet_sequence_num);
        memset(propara.packet_data, 0x0, sizeof(propara.packet_data));
        memcpy(propara.packet_data,
               data + max_data_len * (propara.packet_sequence_num - 1),
               propara.packet_data_len);
        msg.len = Jt808FramePack(msg, DOWN_UPDATEPACKAGE, propara);
        if (SendFrameData(device_it->socket_fd, msg)) {
          close(device_it->socket_fd);
          device_it->socket_fd = -1;
          break;
        } else {
          while (1) {
            // watting for the terminal's response.
            if (RecvFrameData(device_it->socket_fd, msg)) {
              close(device_it->socket_fd);
              device_it->socket_fd = -1;
              break;
            } else if (msg.len > 0) {
              if ((Jt808FrameParse(msg, propara) == UP_UNIRESPONSE) &&
                  (propara.respond_id == DOWN_UPDATEPACKAGE))
                break;
            }
          }
          if (device_it->socket_fd == -1)
            break;
          ++propara.packet_sequence_num;
          usleep(1000);
        }
      }

      if (device_it->socket_fd > 0) {
        EpollRegister(epoll_fd_, device_it->socket_fd);
      }
      delete [] data;
      data = nullptr;
    }
  }
}

