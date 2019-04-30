#ifndef __JT808_SERVICE_H__
#define __JT808_SERVICE_H__

#include <sys/types.h>
#include <sys/epoll.h>
#include <string.h>

#include <string>
#include <list>
#include <mutex>

#include "service/jt808_util.h"
#include "unix_socket/unix_socket.h"

using std::list;

struct DeviceNode {
  bool has_upgrade;
  bool upgrading;
  char phone_num[12];
  char authen_code[8];
  char manufacturer_id[5];
  char upgrade_version[12];
  char upgrade_type;
  char file_path[128];
  int socket_fd;
};

class Jt808Service {
public:
  Jt808Service();
  virtual ~Jt808Service();

  // init service.
  bool Init(const int &port, const int &sock_count);
  bool Init(const char *ip, const int &port, const int &sock_count);
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

  int DealGetStartupRequest(DeviceNode *device, char *result);
  int DealGetGpsRequest(DeviceNode *device, char *result);
  int DealGetCdradioRequest(DeviceNode *device, char *result);
  int DealGetNtripCorsRequest(DeviceNode *device, char *result);
  int DealGetNtripServiceRequest(DeviceNode *device, char *result);
  int DealGetJt808ServiceRequest(DeviceNode *device, char *result);

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
  std::mutex mutex_;
  uid_t uid_;
  MessageData message_;
  list<DeviceNode *> device_list_;
  char file_path[256];
  struct epoll_event *epoll_events_;
};

#endif // JT808_SERVICE_H
