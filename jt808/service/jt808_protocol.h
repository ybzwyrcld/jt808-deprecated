#ifndef JT808_SERVICE_JT808_PROTOCOL_H_
#define JT808_SERVICE_JT808_PROTOCOL_H_

#include <stdint.h>
#include <unistd.h>

#include <list>

#include "service/jt808_area_route.h"
#include "service/jt808_can.h"
#include "service/jt808_passthrough.h"
#include "service/jt808_position_report.h"
#include "service/jt808_terminal_parameters.h"


#define UP_UNIRESPONSE                  0x0001  // 终端通用应答
#define UP_HEARTBEAT                    0x0002  // 终端心跳
#define UP_REGISTER                     0x0100  // 终端注册
#define UP_LOGOUT                       0x0101  // 终端注销
#define UP_AUTHENTICATION               0x0102  // 终端鉴权
#define UP_GETPARARESPONSE              0x0104  // 查询终端参数应答
#define UP_UPDATERESULT                 0x0108  // 终端升级结果
#define UP_POSITIONREPORT               0x0200  // 位置信息上报
#define UP_GETPOSITIONINFORESPONSE      0x0201  // 位置信息查询应答
#define UP_CANBUSDATAUPLOAD             0x0705  // CAN 总线数据上传
#define UP_PASSTHROUGH                  0x0900  // 数据上行透传

#define DOWN_UNIRESPONSE        0x8001  // 平台通用应答
#define DOWN_REGISTERRESPONSE   0x8100  // 终端注册应答
#define DOWN_SETTERMPARA        0x8103  // 设置终端参数
#define DOWN_GETTERMPARA        0x8104  // 查询终端参数
#define DOWN_TERMINALCONTROL    0x8105  // 终端控制
#define DOWN_GETSPECTERMPARA    0x8106  // 查询指定终端参数
#define DOWN_UPDATEPACKAGE      0x8108  // 下发终端升级包
#define DOWN_GETPOSITIONINFO    0x8201  // 查询位置信息
#define DOWN_POSITIONTRACK      0x8202  // 位置跟踪
#define DOWN_SETCIRCULARAREA    0x8600  // 设置圆形区域
#define DOWN_DELCIRCULARAREA    0x8601  // 删除圆形区域
#define DOWN_SETRECTANGLEAREA   0x8602  // 设置矩形区域
#define DOWN_DELRECTANGLEAREA   0x8603  // 删除矩形区域
#define DOWN_SETPOLYGONALAREA   0x8604  // 设置多边形区域
#define DOWN_DELPOLYGONALAREA   0x8605  // 删除多边形区域
#define DOWN_SETROUTE           0x8606  // 设置路线
#define DOWN_DELROUTE           0x8607  // 删除路线
#define DOWN_PASSTHROUGH        0x8900  // 数据下行透传

#define MSGBODY_NOPACKAGE_POS     13
#define MSGBODY_PACKAGE_POS       17
#define PROTOCOL_SIGN             0x7E  // 标识位
#define PROTOCOL_ESCAPE           0x7D  // 转义标识
#define PROTOCOL_ESCAPE_SIGN      0x02  // 0x7e<-->0x7d后紧跟一个0x02
#define PROTOCOL_ESCAPE_ESCAPE    0x01  // 0x7d<-->0x7d后紧跟一个0x01

//
// 理论上一条数据的最大长度:
//       1        +(      16     +    1024     )*2+      1    +      1
// [PROTOCOL_SIGN]+([MSGHEAD LEN]+[MSGBODY LEN])*2+[CHECK SUM]+[PROTOCOL_SIGN]
//
#define MAX_PROFRAMEBUF_LEN    4096

#define LOOP_BUFFER_SIZE       5

enum UniversalResponseResult {
  kSuccess = 0x0,
  kFailure,
  kMessageHasWrong,
  kNotSupport,
  kAlarmHandlingConfirmation,
};

enum RegisterResponseResult {
  kRegisterSuccess = 0x0,
  kVehiclesHaveBeenRegistered,
  kNoSuchVehicleInTheDatabase,
  kTerminalHaveBeenRegistered,
  kNoSuchTerminalInTheDatabase,
};

#pragma pack(push, 1)

struct MessageData {
  uint8_t buffer[MAX_PROFRAMEBUF_LEN];
  int16_t len;
};

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
  CanBusDataTimestamp can_bus_data_timestamp;
  PassThrough *pass_through;
  std::vector<CircularArea *> *circular_area_list;
  std::vector<RectangleArea *> *rectangle_area_list;
  std::vector<PolygonalArea *> *polygonal_area_list;
  std::vector<Route *> *route_list;
  std::vector<CanBusDataItem *> *can_bus_data_item_list;
  std::list<TerminalParameter *> *terminal_parameter_list;
  uint8_t *terminal_parameter_id_buffer;
  uint8_t *area_route_id_buffer;
};

// 消息体属性
union MessageBodyAttr {
  struct Bit {
    uint16_t msglen:10;  // 消息体长度
    uint16_t encrypt:3;  // 数据加密方式, 当此三位都为0, 表示消息体不加密;
                         //   当第10位为1, 表示消息体经过RSA算法加密
    uint16_t package:1;  // 分包标记
    uint16_t retain:2;  // 保留
  }bit;
  uint16_t val;
};

// 消息头内容
struct MessageHead {
  uint16_t id;  // 消息ID
  union MessageBodyAttr attribute;  // 消息体属性
  uint8_t phone[6];  // 终端手机号
  uint16_t msgflownum;  // 消息流水号
  uint16_t totalpackage;  // 消息总包数, 该消息分包后的总包数
  uint16_t packetseq;  // 包序号, 从1开始
};

struct RegisterInfo {
  uint16_t provinceid;
  uint16_t cityid;
  uint8_t manufacturerid[5];
  uint8_t productmodelid[20];
  uint8_t productid[7];
  uint8_t vehicelcolor;
  uint8_t carlicense[10];
};

enum VichelColor {
  BLUE = 1,
  YELLOW,
  BLACK,
  WHITE,
  OTHER
};

#pragma pack(pop)

#endif // JT808_SERVICE_JT808_PROTOCOL_H_
