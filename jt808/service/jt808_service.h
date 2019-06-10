#ifndef JT808_SERVICE_JT808_SERVICE_H_
#define JT808_SERVICE_JT808_SERVICE_H_

#include <sys/epoll.h>
#include <string.h>

#include <list>
#include <string>
#include <vector>

#include "service/jt808_protocol.h"
#include "service/jt808_util.h"
#include "unix_socket/unix_socket.h"


class Jt808Service {
 public:
  Jt808Service() = default;
  Jt808Service(const Jt808Service&) = delete;
  Jt808Service& operator=(const Jt808Service&) = delete;
  virtual ~Jt808Service();

  // Init service.
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
  int DealSetStartupRequest(DeviceNode &device,
                            std::vector<std::string> &va_vec);
  int DealGetGpsRequest(DeviceNode &device, char *result);
  int DealSetGpsRequest(DeviceNode &device,
                        std::vector<std::string> &va_vec);
  int DealGetCdradioRequest(DeviceNode &device, char *result);
  int DealSetCdradioRequest(DeviceNode &device,
                            std::vector<std::string> &va_vec);
  int DealGetNtripCorsRequest(DeviceNode &device, char *result);
  int DealSetNtripCorsRequest(DeviceNode &device,
                              std::vector<std::string> &va_vec);
  int DealGetNtripServiceRequest(DeviceNode &device, char *result);
  int DealSetNtripServiceRequest(DeviceNode &device,
                                 std::vector<std::string> &va_vec);
  int DealGetJt808ServiceRequest(DeviceNode &device, char *result);
  int DealSetJt808ServiceRequest(DeviceNode &device,
                                 std::vector<std::string> &va_vec);
  int DealGetTerminalParameterRequest(DeviceNode &device,
                                      std::vector<std::string> &va_vec);

  int DealSetTerminalParameterRequest(DeviceNode &device,
                                      std::vector<std::string> &va_vec);
  int DealSetCircularAreaRequest(DeviceNode &device,
                                 std::vector<std::string> &va_vec);
  int DealSetRectangleAreaRequest(DeviceNode &device,
                                  std::vector<std::string> &va_vec);
  int DealSetPolygonalAreaRequest(DeviceNode &device,
                                  std::vector<std::string> &va_vec);
  int DealSetRouteRequest(DeviceNode &device, std::vector<std::string> &va_vec);
  int DealDeleteAreaRouteRequest(DeviceNode &device,
                                 std::vector<std::string> &va_vec,
                                 const uint16_t &command);
  int DealGetPositionInfoRequest(DeviceNode &device);
  int DealPositionTrackRequest(DeviceNode &device,
                               std::vector<std::string> &va_vec);
  int DealTerminalControlRequest(DeviceNode &device,
                                 std::vector<std::string> &va_vec);
  int DealVehicleControlRequest(DeviceNode &device,
                                std::vector<std::string> &va_vec);

  int ParseCommand(char *command);

  // Deal upgrade request thread.
  void UpgradeHandler(void);
  static void *StartUpgradeThread(void *arg) {
    Jt808Service *_this = reinterpret_cast<Jt808Service *>(arg);
    _this->UpgradeHandler();
    return nullptr;
  }

 private:
  const char *kDevicesFilePath = "./devicelist/devices.txt";
  const char *kCommandInterfacePath = "/tmp/jt808cmd.sock";

  int listen_sock_ = -1;
  int epoll_fd_ = -1;
  int max_count_ = 0;
  int message_flow_num_ = 0;
  int socket_fd_ = -1;
  int client_fd_ = -1;
  uid_t uid_;
  char file_path[256] = {0};
  std::list<DeviceNode> device_list_;
  struct epoll_event *epoll_events_ = nullptr;
};

#endif // JT808_SERVICE_JT808_SERVICE_H_
