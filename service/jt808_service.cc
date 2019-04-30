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

static int EpollUnRegister(const int &epoll_fd, const int &fd) {
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

static uint8_t BccCheckSum(uint8_t *src, const int &len) {
    int16_t i = 0;
  uint8_t checksum = 0;

  for (i = 0; i < len; ++i) {
    checksum = checksum ^ src[i];
  }

  return checksum;
}

static uint16_t Escape(uint8_t *data, const int &len) {
  uint16_t i, j;

  uint8_t *buf = (uint8_t *) new char[len * 2];
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

  uint8_t *buf = (uint8_t *) new char[len * 2];
  memset(buf, 0x0, len * 2);

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

Jt808Service::Jt808Service() {
  listen_sock_ = 0;
  epoll_fd_ = 0;
  max_count_ = 0;
  epoll_events_ = NULL;
  message_flow_num_ = 0;
}

Jt808Service::~Jt808Service() {
  if(listen_sock_ > 0){
    close(listen_sock_);
  }

  if(epoll_fd_ > 0){
    close(epoll_fd_);
  }
}

static void ReadDevicesList(const char *path, list<DeviceNode *> &list) {
  ifstream ifs;
  string line;
  string content;
  string::size_type spos;
  string::size_type epos;
  stringstream ss;
  DeviceNode *node;
  union {
    uint32_t value;
    uint8_t array[4];
  }code;

  ifs.open(path, std::ios::binary | std::ios::in);
  if (ifs.is_open()) {
    while (getline(ifs, line)) {
      node = new DeviceNode;
      memset(node, 0x0, sizeof(DeviceNode));
      memset(&code, 0x0, sizeof(code));
      ss.clear();
      spos = 0;
      epos = line.find(";");
      content = line.substr(spos, epos - spos);
      content.copy(node->phone_num, content.length(), 0);
      ++epos;
      spos = epos;
      epos = line.find(";", spos);
      content = line.substr(spos, epos - spos);
      ss << content;
      ss >> code.value;
      memcpy(node->authen_code, code.array, 4);
      node->socket_fd = -1;
      list.push_back(node);
      ss.str("");
    }
    ifs.close();
  }

  return ;
}

bool Jt808Service::Init(const int &port, const int &sock_count) {
  max_count_ = sock_count;
  struct sockaddr_in caster_addr;
  memset(&caster_addr, 0, sizeof(struct sockaddr_in));
  caster_addr.sin_family = AF_INET;
  caster_addr.sin_port = htons(port);
  caster_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_sock_ == -1) {
    exit(1);
  }

  if(bind(listen_sock_, (struct sockaddr*)&caster_addr,
          sizeof(struct sockaddr)) == -1){
    exit(1);
  }

  if(listen(listen_sock_, 5) == -1){
    exit(1);
  }

  epoll_events_ = new struct epoll_event[sock_count];
  if (epoll_events_ == NULL){
    exit(1);
  }

  epoll_fd_ = epoll_create(sock_count);
  EpollRegister(epoll_fd_, listen_sock_);

  ReadDevicesList(kDevicesFilePath, device_list_);

  socket_fd_ = ServerListen(kCommandInterfacePath);
  EpollRegister(epoll_fd_, socket_fd_);

  return true;
}

bool Jt808Service::Init(const char *ip,
                        const int &port , const int &sock_count) {
  max_count_ = sock_count;
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock_ == -1) {
    exit(1);
  }

  if (bind(listen_sock_, (struct sockaddr*)&server_addr,
           sizeof(struct sockaddr)) == -1) {
    exit(1);
  }

  if (listen(listen_sock_, 5) == -1){
    exit(1);
  }

  epoll_events_ = new struct epoll_event[sock_count];
  if (epoll_events_ == NULL){
    exit(1);
  }

  epoll_fd_ = epoll_create(sock_count);
  EpollRegister(epoll_fd_, listen_sock_);

  ReadDevicesList(kDevicesFilePath, device_list_);

  socket_fd_ = ServerListen(kCommandInterfacePath);
  EpollRegister(epoll_fd_, socket_fd_);

  return true;
}

int Jt808Service::AcceptNewClient(void) {
  uint16_t command = 0;
  struct sockaddr_in client_addr;
  ProtocolParameters propara;
  MessageData msg;
  char phone_num[6] = {0};
  decltype (device_list_.begin()) it;

  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t clilen = sizeof(struct sockaddr);

  int new_sock = accept(listen_sock_, (struct sockaddr*)&client_addr, &clilen);

  int keepalive = 1;  // enable keepalive attributes.
  int keepidle = 60;  // time out for starting detection.
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
          it = device_list_.begin();
          while (it != device_list_.end()) {
            BcdFromStringCompress((*it)->phone_num,
                                  phone_num, strlen((*it)->phone_num));
            if (memcmp(phone_num, propara.phone_num, 6) == 0)
              break;
            else
              ++it;
          }
          if (it != device_list_.end()) {
            memcpy((*it)->manufacturer_id, propara.manufacturer_id, 5);
            (*it)->socket_fd = new_sock;
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
  char recv_buff[1024] = {0};
  ProtocolParameters propara;
  MessageData msg;

  while(1){
    int ret = Jt808ServiceWait(time_out);
    if(ret == 0){
      //printf("time out\n");
      continue;
    } else {
      for (int i = 0; i < ret; i++) {
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
            if (ParseCommand(recv_buff) > 0)
               send(client_fd_, recv_buff, strlen(recv_buff), 0);
          }
          close(client_fd_);
        } else if ((epoll_events_[i].events & EPOLLIN) &&
                   !device_list_.empty()) {
          auto it = device_list_.begin();
          while (it != device_list_.end()) {
            if (epoll_events_[i].data.fd == (*it)->socket_fd) {
              if (!RecvFrameData(epoll_events_[i].data.fd, msg)) {
                Jt808FrameParse(msg, propara);
              } else {
                EpollUnRegister(epoll_fd_, epoll_events_[i].data.fd);
                close(epoll_events_[i].data.fd);
                (*it)->socket_fd = -1;
              }
              break;
            } else {
              ++it;
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
  MessageHead *msghead_ptr;
  uint8_t *msg_body;
  uint16_t u16temp;
  uint32_t u32temp;

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
        // send authen code if success.
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
        auto termpara_it = propara.terminal_parameter_list->begin();
        while(termpara_it != propara.terminal_parameter_list->end()) {
          u32temp = (*termpara_it)->parameter_id;
          u32temp = EndianSwap32(u32temp);
          memcpy(msg_body, &u32temp, 4);
          msg_body += 4;
          msg.len += 4;
          msghead_ptr->attribute.bit.msglen += 4;
          ++termpara_it;
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

#if 0
  printf("%s[%d]: socket-send:\n", __FILE__, __LINE__);
  for (uint16_t i = 0; i < msg.len; ++i) {
    printf("%02X ", msg.buffer[i]);
  }
  printf("\r\n");
#endif

  return msg.len;
}

uint16_t Jt808Service::Jt808FrameParse(MessageData &msg,
                                       ProtocolParameters &propara) {
  MessageHead *msghead_ptr;
  uint8_t *msg_body;
  char phone_num[12] = {0};
  uint8_t u8temp;
  uint16_t u16temp;
  uint16_t msglen;
  uint32_t u32temp;
  double latitude;
  double longitude;
  float altitude;
  float speed;
  float bearing;
  char timestamp[6];

  msg.len = UnEscape(&msg.buffer[1], msg.len);
  msghead_ptr = (MessageHead *)&msg.buffer[1];
  msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);

  if (msghead_ptr->attribute.bit.package) {  // the flags of divide the package.
    msg_body = &msg.buffer[MSGBODY_PACKAGE_POS];
  } else {
    msg_body = &msg.buffer[MSGBODY_NOPACKAGE_POS];
  }

  msghead_ptr->msgflownum = EndianSwap16(msghead_ptr->msgflownum);
  msghead_ptr->id = EndianSwap16(msghead_ptr->id);
  msglen = static_cast<uint16_t>(msghead_ptr->attribute.bit.msglen);
  switch (msghead_ptr->id) {
    case UP_UNIRESPONSE:
      // message id.
      memcpy(&u16temp, &msg_body[2], 2);
      u16temp = EndianSwap16(u16temp);
      memcpy(&propara.respond_id, &u16temp, 2);
      switch(u16temp) {
        case DOWN_UPDATEPACKAGE:
          printf("%s[%d]: received updatepackage respond: ",
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
      propara.respond_flow_num = msghead_ptr->msgflownum;
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      // check phone num.
      if (!device_list_.empty()) {
        auto device_it = device_list_.begin();
        while (device_it != device_list_.end()) {
          BcdFromStringCompress((*device_it)->phone_num,
                                phone_num,
                                strlen((*device_it)->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++device_it;
        }
        if (device_it != device_list_.end()) { // find phone num.
          // make sure there is no device connection.
          if ((*device_it)->socket_fd == -1) {
            propara.respond_result = 0;
            memcpy(propara.authen_code, (*device_it)->authen_code, 4);
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
      propara.respond_flow_num = msghead_ptr->msgflownum;
      propara.respond_id = msghead_ptr->id;
      memcpy(propara.phone_num, msghead_ptr->phone, 6);
      if (!device_list_.empty()) {
        auto device_it = device_list_.begin();
        while (device_it != device_list_.end()) {
          BcdFromStringCompress((*device_it)->phone_num,
                                phone_num,
                                strlen((*device_it)->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++device_it;
        }
        if ((device_it != device_list_.end()) &&
            (memcmp((*device_it)->authen_code, msg_body, msglen) == 0)) {
          propara.respond_result = 0;
        } else {
          propara.respond_result = 1;
        }
      } else {
        propara.respond_result = 1;
      }
      break;
    case UP_GETPARASPONSE:
      propara.respond_flow_num = msghead_ptr->msgflownum;
      propara.respond_id = msghead_ptr->id;
      if (msg_body[2] != propara.terminal_parameter_list->size()) {
        propara.respond_result = 1;
        break;
      }
      msg_body += 3;
      if ((propara.terminal_parameter_list != nullptr) &&
          !propara.terminal_parameter_list->empty()) {
        stringstream ss;
        auto termpara_it = propara.terminal_parameter_list->begin();
        while(termpara_it != propara.terminal_parameter_list->end()) {
          memcpy(&u32temp, &msg_body[0], 4);
          u32temp = EndianSwap32(u32temp);
          if (u32temp != (*termpara_it)->parameter_id) {
            propara.respond_result = 1;
            return msghead_ptr->id;
          }
          msg_body += 4;
          u8temp = msg_body[0];
          msg_body++;
          switch ((*termpara_it)->parameter_type) {
            case kByteType:
              memcpy(&u32temp, &msg_body[0], u8temp);
              snprintf(reinterpret_cast<char*>(
                  (*termpara_it)->parameter_value),
                  sizeof((*termpara_it)->parameter_value),
                  "%d", msg_body[0]);
              break;
            case kWordType:
              memcpy(&u16temp, &msg_body[0], u8temp);
              u16temp = EndianSwap16(u16temp);
              snprintf(reinterpret_cast<char*>(
                  (*termpara_it)->parameter_value),
                  sizeof((*termpara_it)->parameter_value),
                  "%d", u16temp);
              break;
            case kDwordType:
              memcpy(&u32temp, &msg_body[0], u8temp);
              u32temp = EndianSwap32(u32temp);
              snprintf(reinterpret_cast<char*>(
                  (*termpara_it)->parameter_value),
                  sizeof((*termpara_it)->parameter_value),
                  "%d", u32temp);
              break;
            case kStringType:
              memcpy((*termpara_it)->parameter_value,
                     &msg_body[0], u8temp);
              break;
            default:
              break;
          }
          msg_body += u8temp;
          ++termpara_it;
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

  return msghead_ptr->id;
}

static uint8_t GetParameterTypeByParameterId(const uint32_t &para_id) {
  switch (para_id) {
    case GNSSPOSITIONMODE: case GNSSBAUDERATE: case GNSSOUTPUTFREQ:
    case GNSSUPLOADWAY: case STARTUPGPS: case STARTUPCDRADIO:
    case STARTUPNTRIPCORS: case STARTUPNTRIPSERV: case STARTUPJT808SERV:
    case GPSLOGGGA: case GPSLOGRMC: case GPSLOGATT:
    case CDRADIORECEIVEMODE: case CDRADIOFORMCODE: case NTRIPCORSGGAAUTHEN:
    case NTRIPCORSGGASENDFREQ: case NTRIPSERVICEGGAAUTHEN:
    case NTRIPSERVICEGGASENDFREQ: case JT808SERVICESENDFREQ:
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
                                   std::list<TerminalParameter *> *para_list) {
  if (para_list == nullptr)
    return ;

  TerminalParameter *node = new TerminalParameter;
  memset(node, 0x0, sizeof(*node));
  node->parameter_id = para_id;
  node->parameter_type = GetParameterTypeByParameterId(para_id);
  node->parameter_len = GetParameterLengthByParameterType(
                             node->parameter_type);
  para_list->push_back(node);

  return ;
}

template <class T>
static void ClearListElement(std::list<T *> &list) {
  if (list.empty())
    return ;

  auto remove_it = list.begin();
  while (remove_it != list.end()) {
    delete (*remove_it);
    remove_it = list.erase(remove_it);
  }

  return ;
}

template <class T>
static void ClearListElement(std::list<T *> *list) {
  if (list == nullptr || list->empty())
    return ;

  auto remove_it = list->begin();
  while (remove_it != list->end()) {
    delete (*remove_it);
    remove_it = list->erase(remove_it);
  }

  delete list;
  list = nullptr;

  return ;
}

int Jt808Service::DealGetStartupRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(STARTUPGPS,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(STARTUPCDRADIO,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(STARTUPNTRIPCORS,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(STARTUPNTRIPSERV,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(STARTUPJT808SERV,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto result_it = propara.terminal_parameter_list->begin();
        uint8_t value = 0;
        string str = "startup info:";
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [gps]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [cdradio]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [ntrip cors]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [ntrip servcie]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [jt808 service]" : "";
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

int Jt808Service::DealGetGpsRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(GPSLOGGGA,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(GPSLOGRMC,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(GPSLOGATT,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto result_it = propara.terminal_parameter_list->begin();
        uint8_t value = 0;
        string str = "gps info:";
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [LOGGGA]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [LOGRMC]" : "";
        ++result_it;
        value = (*result_it)->parameter_value[0];
        str += value == '1' ? " [LOGATT]" : "";
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

int Jt808Service::DealGetCdradioRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(CDRADIOBAUDERATE,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(CDRADIOWORKINGFREQ,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(CDRADIORECEIVEMODE,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(CDRADIOFORMCODE,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        string str = "cdradio info: bdrt[";
        auto result_it = propara.terminal_parameter_list->begin();
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],freq[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],recvmode[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],formcode[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
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

int Jt808Service::DealGetNtripCorsRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(NTRIPCORSIP,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSPORT,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSUSERNAME,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSPASSWD,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSMOUNTPOINT,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSGGAAUTHEN,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPCORSGGASENDFREQ,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        string str = "ntrip cors info: ip[";
        auto result_it = propara.terminal_parameter_list->begin();
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],port[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],username[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],password[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],mountpoint[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        ++result_it;
        uint8_t value = (*result_it)->parameter_value[0];
        str += value == '1' ? "],[GGAAUTHEN" : "";
        str +="],sendfreq[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
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

int Jt808Service::DealGetNtripServiceRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(NTRIPSERVICEIP,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEPORT,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEUSERNAME,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEPASSWD,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEMOUNTPOINT,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEGGAAUTHEN,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(NTRIPSERVICEGGASENDFREQ,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        string str = "ntrip service info: ip[";
        auto result_it = propara.terminal_parameter_list->begin();
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],port[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],username[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],password[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],mountpoint[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        ++result_it;
        uint8_t value = (*result_it)->parameter_value[0];
        str += value == '1' ? "],[GGAAUTHEN" : "";
        ++result_it;
        str +="],sendfreq[";
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
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

int Jt808Service::DealGetJt808ServiceRequest(DeviceNode *device, char *result) {
  int retval = 0;
  ProtocolParameters propara;
  MessageData msg;

  memset(&msg, 0x0, sizeof(msg));
  memset(&propara, 0x0, sizeof(propara));
  propara.terminal_parameter_list = new std::list<TerminalParameter *>;

  AddParameterNodeByParameterId(JT808SERVICEIP,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(JT808SERVICEPORT,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(JT808SERVICEPHONENUM,
                                propara.terminal_parameter_list);
  AddParameterNodeByParameterId(JT808SERVICESENDFREQ,
                                propara.terminal_parameter_list);

  Jt808FramePack(msg, DOWN_GETSPECTERMPARA, propara);
  SendFrameData(device->socket_fd, msg);
  while (1) {
    memset(&msg, 0x0, sizeof(msg));
    if (RecvFrameData(device->socket_fd, msg)) {
      close(device->socket_fd);
      break;
    } else if (msg.len > 0) {
      if (Jt808FrameParse(msg, propara) == UP_GETPARASPONSE) {
        auto result_it = propara.terminal_parameter_list->begin();
        string str = "jt808 service info: ip[";
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],port[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str += "],phone[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
        str +="],uploadfreq[";
        ++result_it;
        str += reinterpret_cast<char *>((*result_it)->parameter_value);
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

int Jt808Service::ParseCommand(char *command) {
  int retval = 0;
  string command_para;
  string phone_num;
  stringstream ss;

  ss.clear();
  ss.str("");
  ss << command;
  ss >> command_para;
  if (!device_list_.empty()) {
    auto it = device_list_.begin();
    while (it != device_list_.end()) {
      phone_num = (*it)->phone_num;
      if (command_para == phone_num) { // find node.
        break;
      } else {
        ++it;
      }
    }
    if (it != device_list_.end() && ((*it)->socket_fd > 0)) {
      ss >> command_para;
      if (command_para == "upgrade") {
        ss >> command_para;
        if ((command_para == "device") || (command_para == "gps") ||
            (command_para == "system") || (command_para == "cdradio")) {
          if (command_para == "device")
            (*it)->upgrade_type = 0x0;
          else if (command_para == "gps")
            (*it)->upgrade_type = 0x34;
          else if (command_para == "cdradio")
            (*it)->upgrade_type = 0x35;
          else if (command_para == "system")
            (*it)->upgrade_type = 0x36;
          else
            return -1;

          ss >> command_para;
          memset((*it)->upgrade_version, 0x0, sizeof((*it)->upgrade_version));
          command_para.copy((*it)->upgrade_version, command_para.length(), 0);

          ss >> command_para;
          memset((*it)->file_path, 0x0, sizeof((*it)->file_path));
          command_para.copy((*it)->file_path, command_para.length(), 0);
          (*it)->has_upgrade = true;

          // start upgrade thread.
          std::thread start_upgrade_thread(StartUpgradeThread, this);
          start_upgrade_thread.detach();
        }
      } else if (command_para == "get") {
        EpollUnRegister(epoll_fd_, (*it)->socket_fd);
        memset(command, 0x0, strlen(command));
        // type.
        ss >> command_para;
        if (command_para == "startup") {
          retval = DealGetStartupRequest(*it, command);
        } else if (command_para == "gps") {
          retval = DealGetGpsRequest(*it, command);
        } else if (command_para == "cdradio") {
          retval = DealGetCdradioRequest(*it, command);
        } else if (command_para == "ntripcors") {
          retval = DealGetNtripCorsRequest(*it, command);
        } else if (command_para == "ntripservice") {
          retval = DealGetNtripServiceRequest(*it, command);
        } else if (command_para == "jt808service") {
          retval = DealGetJt808ServiceRequest(*it, command);
        }
        EpollRegister(epoll_fd_, (*it)->socket_fd);
      }
    }
  }

  ss.str("");
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
    auto it = device_list_.begin();
    while (it != device_list_.end()) {
      if ((*it)->has_upgrade) {
        (*it)->has_upgrade = false;
        break;
      } else {
        ++it;
      }
    }

    if (it == device_list_.end())
      return ;

    memset(&propara, 0x0, sizeof(propara));
    memcpy(propara.version_num,
           (*it)->upgrade_version, strlen((*it)->upgrade_version));
    max_data_len = 1023 - 11 - strlen((*it)->upgrade_version);
    EpollUnRegister(epoll_fd_, (*it)->socket_fd);
    //printf("path: %s\n", file_path);
    ifs.open((*it)->file_path, std::ios::binary | std::ios::in);
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
      propara.upgrade_type = (*it)->upgrade_type;
      // upgrade version name len.
      propara.version_num_len = strlen((*it)->upgrade_version);
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
        if (SendFrameData((*it)->socket_fd, msg)) {
          close((*it)->socket_fd);
          (*it)->socket_fd = -1;
          break;
        } else {
          while (1) {
            // watting for the terminal's response.
            if (RecvFrameData((*it)->socket_fd, msg)) {
              close((*it)->socket_fd);
              (*it)->socket_fd = -1;
              break;
            } else if (msg.len > 0) {
              if ((Jt808FrameParse(msg, propara) == UP_UNIRESPONSE) &&
                  (propara.respond_id == DOWN_UPDATEPACKAGE))
                break;
            }
          }

          if ((*it)->socket_fd == -1)
            break;

          ++propara.packet_sequence_num;
          usleep(1000);
        }
      }

      if ((*it)->socket_fd > 0) {
        EpollRegister(epoll_fd_, (*it)->socket_fd);
      }

      delete [] data;
      data = nullptr;
    }
  }

  return ;
}

