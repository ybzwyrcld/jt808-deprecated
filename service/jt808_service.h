#ifndef __JT808_SERVICE_H__
#define __JT808_SERVICE_H__

#include <sys/types.h>
#include <sys/epoll.h>
#include <string.h>

#include <string>
#include <list>
#include <vector>

#include "service/jt808_util.h"
#include "unix_socket/unix_socket.h"

using std::string;
using std::list;
using std::vector;

struct DeviceNode {
  bool has_upgrade;
  bool upgrading;
  char phone_num[12];
  char authen_code[8];
  char manufacturer_id[5];
  char upgrade_version[12];
  char upgrade_type;
  char file_path[256];
  int socket_fd;
};

class Jt808Service {
 public:
  Jt808Service() {
    listen_sock_ = 0;
    epoll_fd_ = 0;
    max_count_ = 0;
    message_flow_num_ = 0;
    epoll_events_ = nullptr;
  }

  virtual ~Jt808Service() {
    if(listen_sock_ > 0){
      close(listen_sock_);
    }
    if(epoll_fd_ > 0){
      close(epoll_fd_);
    }
    device_list_.clear();
    delete [] epoll_events_;
  }

  // init service.
  bool Init(const int &port, const int &max_count);
  bool Init(const char *ip, const int &port, const int &max_count);
  int AcceptNewClient(void);

  // accept when command client connect.
  int AcceptNewCommandClient(void) {
    memset(&uid_, 0x0, sizeof(uid_));
    client_fd_ = ServerAccept(socket_fd_, &uid_);
    return client_fd_;
  }

  int Jt808ServiceWait(const int &time_out) {
    return epoll_wait(epoll_fd_, epoll_events_, max_count_, time_out);
  }

  void Run(const int &time_out);

  int SendFrameData(const int &fd, const MessageData &msg);
  int RecvFrameData(const int &fd, MessageData &msg);

  int Jt808FramePack(MessageData &msg, const uint16_t &command,
                     const ProtocolParameters &propara);

  uint16_t Jt808FrameParse(MessageData &msg, ProtocolParameters &propara);

  int DealGetStartupRequest(DeviceNode &device, char *result);
  int DealSetStartupRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetGpsRequest(DeviceNode &device, char *result);
  int DealSetGpsRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetCdradioRequest(DeviceNode &device, char *result);
  int DealSetCdradioRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetNtripCorsRequest(DeviceNode &device, char *result);
  int DealSetNtripCorsRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetNtripServiceRequest(DeviceNode &device, char *result);
  int DealSetNtripServiceRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetJt808ServiceRequest(DeviceNode &device, char *result);
  int DealSetJt808ServiceRequest(DeviceNode &device, vector<string> &va_vec);
  int DealGetTerminalParameterRequest(DeviceNode &device,
                                      vector<string> &va_vec, char *result);
  int DealSetTerminalParameterRequest(DeviceNode &device,
                                      vector<string> &va_vec);

  int ParseCommand(char *command);

  // upgrade thread.
  void UpgradeHandler(void);
  static void *StartUpgradeThread(void *arg) {
    Jt808Service *_this = reinterpret_cast<Jt808Service *>(arg);
    _this->UpgradeHandler();
    return nullptr;
  }

 private:
  const char *kDevicesFilePath = "./devices/devices.list";
  const char *kCommandInterfacePath = "/tmp/jt808cmd.sock";

  int listen_sock_;
  int epoll_fd_;
  int max_count_;
  int message_flow_num_;
  int socket_fd_;
  int client_fd_;
  uid_t uid_;
  MessageData message_;
  list<DeviceNode> device_list_;
  char file_path[256];
  struct epoll_event *epoll_events_;
};

#endif // JT808_SERVICE_H
