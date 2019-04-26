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
    } else if ((data[i] == PROTOCOL_ESCAPE) && (data[i+1] == PROTOCOL_ESCAPE_ESCAPE)) {
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

static void ReadDevicesList(const char *path, list<Node> &list) {
  ifstream ifs;
  string line;
  string content;
  string::size_type spos;
  string::size_type epos;
  stringstream ss;
  Node node;
  union {
    uint32_t value;
    uint8_t array[4];
  }code;

  memset(&node, 0x0, sizeof(node));
  ifs.open(path, std::ios::binary | std::ios::in);
  if (ifs.is_open()) {
    while (getline(ifs, line)) {
      memset(&node, 0x0, sizeof(Node));
      memset(&code, 0x0, sizeof(code));
      ss.clear();
      spos = 0;
      epos = line.find(";");
      content = line.substr(spos, epos - spos);
      content.copy(node.phone_num, content.length(), 0);
      ++epos;
      spos = epos;
      epos = line.find(";", spos);
      content = line.substr(spos, epos - spos);
      ss << content;
      ss >> code.value;
      memcpy(node.authen_code, code.array, 4);
      node.socket_fd = -1;
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

  ReadDevicesList(kDevicesFilePath, list_);

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

  ReadDevicesList(kDevicesFilePath, list_);

  socket_fd_ = ServerListen(kCommandInterfacePath);
  EpollRegister(epoll_fd_, socket_fd_);

  return true;
}

int Jt808Service::AcceptNewClient(void) {
  uint16_t command = 0;
  struct sockaddr_in client_addr;
  Node node;
  ProtocolParameters propara;
  MessageData msg;
  char phone_num[6] = {0};
  decltype (list_.begin()) it;

  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t clilen = sizeof(struct sockaddr);

  int new_sock = accept(listen_sock_, (struct sockaddr*)&client_addr, &clilen);

  int keepalive = 1;  // enable keepalive attributes.
  int keepidle = 60;  // time out for starting detection.
  int keepinterval = 5;  // time interval for sending packets during detection.
  int keepcount = 3;  // max times for sending packets during detection.

  setsockopt(new_sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPINTVL, &keepinterval, sizeof(keepinterval));
  setsockopt(new_sock, SOL_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));

  if (!RecvFrameData(new_sock, message_)) {
    command = Jt808FrameParse(message_, propara);
    switch (command) {
      case UP_REGISTER:
        memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
        msg.len = Jt808FramePack(msg, DOWN_REGISTERRSPONSE, propara);
        SendFrameData(new_sock, msg);
        if (propara.bytepara1 != 0x0) {
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
        if (propara.bytepara1 != 0x0) {
          close(new_sock);
          new_sock = -1;
        } else if (!list_.empty()) {
          it = list_.begin();
          while (it != list_.end()) {
            BcdFromStringCompress(it->phone_num,
                                  phone_num, strlen(it->phone_num));
            if (memcmp(phone_num, propara.stringpara1, 6) == 0)
              break;
            else
              ++it;
          }
          if (it != list_.end()) {
            it->socket_fd = new_sock;
            node = *it;
            list_.erase(it);
            list_.push_back(node);
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
  Node node;
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
            ParseCommand(recv_buff);
          } else if (ret == 0) {
            EpollUnRegister(epoll_fd_, client_fd_);
            close(client_fd_);
          }
        } else if ((epoll_events_[i].events & EPOLLIN) && !list_.empty()) {
          auto it = list_.begin();
          while (it != list_.end()) {
            if (epoll_events_[i].data.fd == it->socket_fd) {
              if (!RecvFrameData(epoll_events_[i].data.fd, msg)) {
                Jt808FrameParse(msg, propara);
              } else {
                EpollUnRegister(epoll_fd_, epoll_events_[i].data.fd);
                close(epoll_events_[i].data.fd);
                it->socket_fd = -1;
                node = *it;
                list_.erase(it);
                list_.push_back(node);;
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
  size_t len;

  msghead_ptr = (MessageHead *)&msg.buffer[1];
  msghead_ptr->id = EndianSwap16(command);
  msghead_ptr->attribute.val = 0x0000;
  msghead_ptr->attribute.bit.encrypt = 0;
  if (propara.wordpara3 > 1) {  // need to divide the package.
    msghead_ptr->attribute.bit.package = 1;
    msg_body = &msg.buffer[MSGBODY_PACKAGE_POS];
    // packet total num.
    u16temp = propara.wordpara3;
    u16temp = EndianSwap16(u16temp);
    memcpy(&msg.buffer[13], &u16temp, 2);
    // packet sequence num.
    u16temp = propara.wordpara4;
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
  memcpy(msghead_ptr->phone, propara.stringpara1, 6);

  switch (command) {
    case DOWN_UNIRESPONSE:
      // terminal message flow num.
      u16temp = EndianSwap16(propara.wordpara1);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // terminal message id.
      u16temp = EndianSwap16(propara.wordpara2);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // result.
      *msg_body = propara.bytepara1;
      msg_body++;
      msghead_ptr->attribute.bit.msglen = 5;
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      msg.len += 5;
      break;
    case DOWN_REGISTERRSPONSE:
      // terminal message flow num.
      u16temp = EndianSwap16(propara.wordpara1);
      memcpy(msg_body, &u16temp, 2);
      msg_body += 2;
      // result.
      *msg_body = propara.bytepara1;
      msg_body++;
      if (propara.bytepara1 == 0) {
        // send authen code if success.
        memcpy(msg_body, propara.stringpara2, 4);
        msg_body += 4;
        msghead_ptr->attribute.bit.msglen = 7;
        msg.len += 7;
      } else {
        msghead_ptr->attribute.bit.msglen = 3;
        msg.len += 3;
      }
      msghead_ptr->attribute.val = EndianSwap16(msghead_ptr->attribute.val);
      break;
    case DOWN_UPDATEPACKAGE:
      // upgrade type.
      *msg_body = propara.bytepara1;
      msg_body ++;
      msg_body += 5;
      msg.len += 6;
      // length of upgrade version name.
      *msg_body = propara.bytepara2;
      msg_body++;
      msg.len++;
      // upgrade version name.
      memcpy(msg_body, propara.stringpara2, sizeof(propara.stringpara2));
      len = strlen((char *)(propara.stringpara2));
      msg_body += len;
      msg.len += len;
      // length of valid content.
      u32temp = EndianSwap32(propara.wordpara1);
      memcpy(msg_body, &u32temp, 4);
      msg_body += 4;
      msg.len += 4;
      // valid content of the upgrade file.
      memcpy(msg_body, propara.stringpara3, propara.wordpara1);
      msg_body += propara.wordpara1;
      msg.len += propara.wordpara1;
      msghead_ptr->attribute.bit.msglen = 11 + len + propara.wordpara1;
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
  uint16_t u16temp;
  uint16_t msglen;
  uint32_t u32temp;
  double latitude;
  double longitude;
  float altitude;
  float speed;
  float bearing;
  char timestamp[6];
  decltype (list_.begin()) it;

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
    case UP_REGISTER:
      memset(&propara, 0x0, sizeof(propara));
      propara.wordpara1 = msghead_ptr->msgflownum;
      memcpy(propara.stringpara1, msghead_ptr->phone, 6);
      // check phone num.
      if (!list_.empty()) {
      it = list_.begin();
        while (it != list_.end()) {
          BcdFromStringCompress(it->phone_num,
                                phone_num, strlen(it->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++it;
        }
        if (it != list_.end()) { // find phone num.
          if (it->socket_fd == -1) { // make sure there is no device connection.
            propara.bytepara1 = 0;
            memcpy(propara.stringpara2, it->authen_code, 4);
          } else {
            propara.bytepara1 = 3;
          }
        } else {
          propara.bytepara1 = 4;
        }
      } else {
        propara.bytepara1 = 2;
      }
      break;
    case UP_AUTHENTICATION:
      memset(&propara, 0x0, sizeof(propara));
      propara.wordpara1 = msghead_ptr->msgflownum;
      propara.wordpara2 = msghead_ptr->id;
      memcpy(propara.stringpara1, msghead_ptr->phone, 6);
      if (!list_.empty()) {
        it = list_.begin();
        while (it != list_.end()) {
          BcdFromStringCompress(it->phone_num,
                                phone_num, strlen(it->phone_num));
          if (memcmp(phone_num, msghead_ptr->phone, 6) == 0)
            break;
          else
            ++it;
        }
        if ((it != list_.end()) &&
            (memcmp(it->authen_code, msg_body, msglen) == 0)) {
          propara.bytepara1 = 0;
        } else {
          propara.bytepara1 = 1;
        }
      } else {
        propara.bytepara1 = 1;
      }
      break;
    case UP_UNIRESPONSE:
      // message id.
      memcpy(&u16temp, &msg_body[2], 2);
      u16temp = EndianSwap16(u16temp);
      memcpy(&propara.wordpara2, &u16temp, 2);
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

int Jt808Service::ParseCommand(const char *command) {
  string command_para;
  string phone_num;
  stringstream ss;
  decltype (list_.begin()) it;

  ss.clear();
  ss << command;
  ss >> command_para;
  if (command_para == "update") {
    ss >> command_para;
    if (!list_.empty()) {
      it = list_.begin();
      while (it != list_.end()) {
        phone_num = it->phone_num;
        if (command_para == phone_num) {
          printf("find phone num\n");
          break;
        } else {
          ++it;
        }
      }
      if (it != list_.end()) {
        ss >> command_para;
        if ((command_para == "device") || (command_para == "gpsfw") ||
            (command_para == "system") || (command_para == "cdrfw")) {
          if (command_para == "device")
            it->upgrade_type = 0x0;
          else if (command_para == "gpsfw")
            it->upgrade_type = 0x34;
          else if (command_para == "cdrfw")
            it->upgrade_type = 0x35;
          else if (command_para == "system")
            it->upgrade_type = 0x36;
          else
            return -1;

          ss >> command_para;
          memset(it->upgrade_version, 0x0, sizeof(it->upgrade_version));
          command_para.copy(it->upgrade_version, command_para.length(), 0);

          ss >> command_para;
          memset(it->file_path, 0x0, sizeof(it->file_path));
          command_para.copy(it->file_path, command_para.length(), 0);
          it->has_upgrade = true;

          Node node = *it;
          list_.erase(it);
          list_.push_back(node);

          /* start thread. */
          std::thread start_upgrade_thread(StartUpgradeThread, this);
          start_upgrade_thread.detach();
        }
      }
    }
    ss.str("");
  }


  return 0;
}

void Jt808Service::UpgradeHandler(void) {
  MessageData msg;
  ProtocolParameters propara;
  ifstream ifs;
  char *data = nullptr;
  uint32_t len, max_data_len;
  decltype (list_.begin()) it;
  Node node;

  memset(&node, 0x0, sizeof(node));
  if (!list_.empty()) {
    it = list_.begin();
    while (it != list_.end()) {
      if (it->has_upgrade) {
        node = *it;
        list_.erase(it);
        break;
      } else {
        ++it;
      }
    }
  }

  if (strlen(node.phone_num) == 0)
    return ;

  memcpy(propara.stringpara2, node.upgrade_version, strlen(node.upgrade_version));
  max_data_len = 1023 - 11 - strlen(node.upgrade_version);
  EpollUnRegister(epoll_fd_, node.socket_fd);
  //printf("path: %s\n", file_path);
  ifs.open(node.file_path, std::ios::binary | std::ios::in);
  if (ifs.is_open()) {
    ifs.seekg(0, std::ios::end);
    len = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    data = new char[len];
    ifs.read(data, len);
    ifs.close();

    // packet total num.
    propara.wordpara3 = len/max_data_len + 1;
    // packet sequence num.
    propara.wordpara4 = 1;
    // upgrade type.
    propara.bytepara1 = node.upgrade_type;
    // upgrade version name len.
    propara.bytepara2 = strlen(node.upgrade_version);
    // valid content of the upgrade file.
    propara.stringpara3 = new unsigned char [1024];
    while (len > 0) {
      memset(msg.buffer, 0x0, MAX_PROFRAMEBUF_LEN);
      if (len > max_data_len)
        propara.wordpara1 = max_data_len; // length of valid content.
      else
        propara.wordpara1 = len;
      len -= propara.wordpara1;
      //printf("sum packet = %d, sub packet = %d\n",
      //       propara.wordpara3, propara.wordpara4);
      memset(propara.stringpara3, 0x0, 1024);
      memcpy(propara.stringpara3,
             data + max_data_len * (propara.wordpara4 - 1), propara.wordpara1);
      msg.len = Jt808FramePack(msg, DOWN_UPDATEPACKAGE, propara);
      if (SendFrameData(node.socket_fd, msg)) {
        close(node.socket_fd);
        node.socket_fd = -1;
        list_.push_back(node);
        break;
      } else {
        while (1) {
          if (RecvFrameData(node.socket_fd, msg)) {
            close(node.socket_fd);
            node.socket_fd = -1;
            break;
          } else if (msg.len > 0) {
            if ((Jt808FrameParse(msg, propara) == UP_UNIRESPONSE) &&
                (propara.wordpara2 == DOWN_UPDATEPACKAGE))
            break;
          }
        }

        if (node.socket_fd == -1)
          break;

        ++propara.wordpara4;
        usleep(1000);
      }
    }

    if (node.socket_fd > 0) {
      EpollRegister(epoll_fd_, node.socket_fd);
    }

    delete [] data;
    delete [] propara.stringpara3;
    data = nullptr;
  }

  list_.push_back(node);

  return ;
}

