#ifndef JT808_SERVICE_JT808_PROTOCOL_H_
#define JT808_SERVICE_JT808_PROTOCOL_H_

#include <stdint.h>
#include <unistd.h>

#include <list>

#include "service/jt808_terminal_parameters.h"


#define UP_UNIRESPONSE       0x0001  // 终端通用应答
#define UP_HEARTBEAT         0x0002  // 终端心跳
#define UP_REGISTER          0x0100  // 终端注册
#define UP_LOGOUT            0x0101  // 终端注销
#define UP_AUTHENTICATION    0x0102  // 终端鉴权
#define UP_GETPARASPONSE     0x0104  // 查询终端参数应答
#define UP_UPDATERESULT      0x0108  // 终端升级结果
#define UP_POSITIONREPORT    0x0200  // 位置信息上报
#define UP_PASSTHROUGH       0x0900  // 数据上行透传

#define DOWN_UNIRESPONSE        0x8001  // 平台通用应答
#define DOWN_REGISTERRSPONSE    0x8100  // 终端注册应答
#define DOWN_SETTERMPARA        0x8103  // 设置终端参数
#define DOWN_GETTERMPARA        0x8104  // 查询终端参数
#define DOWN_GETSPECTERMPARA    0x8106  // 查询指定终端参数
#define DOWN_UPDATEPACKAGE      0x8108  // 下发终端升级包
#define DOWN_PASSTHROUGH        0x8900  // 数据下行透传

#define MSGBODY_NOPACKAGE_POS     13
#define MSGBODY_PACKAGE_POS       17
#define PROTOCOL_SIGN             0x7E  // 标识位
#define PROTOCOL_ESCAPE           0x7D  // 转义标识
#define PROTOCOL_ESCAPE_SIGN      0x02  // 0x7e<-->0x7d后紧跟一个0x02
#define PROTOCOL_ESCAPE_ESCAPE    0x01  // 0x7d<-->0x7d后紧跟一个0x01


// 里程, 1/10km, 对应车上里程表读数, DWORD
#define POSITIONEXTENSIONMILEAGE            0x01
// 油量, 1/10L, 对应车上油量表读数, WORD
#define POSITIONEXTENSIONOLIQUANTITY        0x02
// 行驶记录功能获取的速度, 1/10km/h, WORD
#define POSITIONEXTENSIONTACHOGRAPHSPEED    0x03
// 需要人工确认报警事件的 ID, 从 1 开始计数, WORD
#define POSITIONEXTENSIONALARMCOUNT         0x04
// 超速报警附加信息, BYTE or BYTE+DWORD
#define POSITIONEXTENSIONOVERSPEEDALARM     0x11
// 进出区域/路线报警附加信息, BYTE+DWORD+BYTE
#define POSITIONEXTENSIONACCESSAREAALARM    0x12
// 路段行驶时间不足/过长报警附加信息, DWORD+WORD+BYTE
#define POSITIONEXTENSIONDRIVINGTIMEALARM   0x13
// 扩展车辆信号状态位, DWORD
#define POSITIONEXTENSIONVEHICLESIGNAL      0x25
// IO 状态位, WORD
#define POSITIONEXTENSIONIOSTATUS           0x2A
// 模拟量, DWORD
#define POSITIONEXTENSIONANALOGQUANTITY     0x2B
// 无线通信网络信号强度, BYTE
#define POSITIONEXTENSIONNETWORKQUALITY     0x30
// GNSS 定位卫星数, BYTE
#define POSITIONEXTENSIONNETWORKQUALITY     0x30
// 后续自定义信息长度, BYTE
#define POSITIONEXTENSIONCUSTOMITEMLENGTH   0xE0
// 定位状态, BYTE
#define POSITIONEXTENSIONPOSITIONSTATUS     0xEE

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
  uint8_t terminal_parameter_id_count;
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
  std::list<TerminalParameter *> *terminal_parameter_list;
  uint8_t *terminal_parameter_id_buffer;
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

// 报警标志位
union AlarmBit {
  struct Bit {
    uint32_t sos:1;  // 紧急报瞥触动报警开关后触发
    uint32_t overspeed:1;  // 超速报警
    uint32_t fatigue:1;  // 疲劳驾驶
    uint32_t earlywarning:1;  // 预警
    uint32_t gnssfault:1;  // GNSS模块发生故障
    uint32_t gnssantennacut:1;  // GNSS天线未接或被剪断
    uint32_t gnssantennashortcircuit:1;  // GNSS天线短路
    uint32_t powerlow:1;  // 终端主电源欠压

    uint32_t powercut:1;  // 终端主电源掉电
    uint32_t lcdfault:1;  // 终端LCD或显示器故障
    uint32_t ttsfault:1;  // TTS模块故障
    uint32_t camerafault:1;  // 摄像头故障
    uint32_t obddtc:1;  // OBD故障码
    uint32_t retain1:5;  // 保留

    uint32_t daydriveovertime:1;  // 当天累计驾驶超时
    uint32_t stopdrivingovertime:1;  // 超时停车
    uint32_t inoutarea:1;  // 进出区域
    uint32_t inoutroad:1;  // 进出路线
    uint32_t roaddrivetime:1;  // 路段行驶时间不足/过长
    uint32_t roaddeviate:1;   // 路线偏离报警
    uint32_t vssfault:1;  // 车辆VSS故障
    uint32_t oilfault:1;  // 车辆油量异常
    uint32_t caralarm:1;  // 车辆被盗(通过车辆防盗器)
    uint32_t caraccalarm:1;  // 车辆非法点火
    uint32_t carmove:1;  // 车辆非法位移
    uint32_t collision:1;  // 碰撞侧翻报警
    uint32_t retain2:2;  // 保留
  }bit;
  uint32_t value;
};

// 状态位
union StatusBit {
  struct Bit {
    uint32_t acc:1;  // ACC 0:ACC关; 1:ACC开
    uint32_t location:1;  // 定位 0:未定位; 1:定位
    uint32_t snlatitude:1;  // 0:北纬: 1:南纬
    uint32_t ewlongitude:1;  // 0:东经; 1:西经
    uint32_t operation:1;  // 0:运营状态; 1:停运状态
    uint32_t gpsencrypt:1;  // 0:经纬度未经保密插件加密;
                            // 1:经纬度已经保密插件加密
    uint32_t retain1:2;      // 保留
    uint32_t trip_stat:2;  // 00: 空车; 01: 半载; 10: 保留; 11: 满载;
    uint32_t oilcut:1;  // 0:车辆油路正常; 1:车辆油路断开
    uint32_t circuitcut:1;  // 0:车辆电路正常; 1:车辆电路断开
    int32_t doorlock:1;  // 0:车门解锁; 1: 车门加锁
    uint32_t door1status:1;  // 0:门 1 关; 1: 门 1 开; (前门)
    uint32_t door2status:1;  // 0:门 2 关; 1: 门 2 开; (中门)
    uint32_t door3status:1;  // 0:门 3 关; 1: 门 3 开; (后门)
    uint32_t door4status:1;  // 0:门 4 关; 1: 门 4 开; (驾驶席门)
    uint32_t door5status:1;  // 0:门 5 关; 1: 门 5 开; (自定义)
    uint32_t gpsen:1;  // 0: 未使用 GPS 卫星进行定位;
                       // 1: 使用 GPS 卫星进行定位;
    uint32_t beidouen:1;  // 0: 未使用北斗卫星进行定位;
                          // 1: 使用北斗卫星进行定位;
    uint32_t glonassen:1;  // 0: 未使用 GLONASS 卫星进行定位;
                           // 1: 使用 GLONASS 卫星进行定位;
    uint32_t galileoen:1;  // 0: 未使用 Galileo 卫星进行定位;
                           // 1: 使用 Galileo 卫星进行定位;
    uint32_t retain2:10;  // 保留
  }bit;
  uint32_t value;
};

// 位置基本信息数据
struct PositionBasicInfo {
  union AlarmBit alarm;
  union StatusBit status;
  // 纬度(以度为单位的纬度值乘以10的6次方, 精确到百万分之一度)
  uint32_t latitude;
  // 经度(以度为单位的纬度值乘以10的6次方, 精确到百万分之一度)
  uint32_t longitude;
  // 海拔高度, 单位为米(m)
  uint16_t atitude;
  // 速度 1/10km/h
  uint16_t speed;
  // 方向 0-359,正北为0, 顺时针
  uint16_t bearing;
  // 时间 BCD[6] YY-MM-DD-hh-mm-ss(GMT+8时间, 本标准之后涉及的时间均采用此时区)
  uint8_t time[6];
};

// 位置附加信息项
struct PositionExtensionItem {
  uint8_t item_id;
  uint8_t item_len;
  uint8_t item_value[8];
};

// 扩展车辆信号状态位
union ExtendedVehicleSignalBit {
  struct Bit {
    uint32_t nearlamp:1;  // 近光灯信号
    uint32_t farlamp:1;  // 远光灯信号
    uint32_t rightturnlamp:1;  // 右转向灯信号
    uint32_t leftturnlamp:1;  // 左转向灯信号
    uint32_t breaking:1;  // 制动信号
    uint32_t reversing:1;  // 倒档信号
    uint32_t foglamp:1;  // 雾灯信号
    uint32_t outlinelamp:1;  // 示廓灯
    uint32_t horn:1;  // 喇叭信号
    uint32_t airconditioner:1;  // 空调状态
    uint32_t neutral:1;  // 空挡信号
    uint32_t retarder:1; // 缓速器工作
    uint32_t abs:1;  // ABS 工作
    uint32_t heater:1;  // 加热器工作
    uint32_t clutch:1;  // 离合器状态
    uint32_t retain:17;  // 保留
  }bit;
  uint32_t value;
};

// IO 状态位
union IoStatusBit {
  struct Bit {
    uint16_t deepdormancy:1;  // 深度休眠状态
    uint16_t dormancy:1;  // 深度休眠状态
    uint32_t retain:14;  // 保留
  }bit;
  uint16_t value;
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

struct Sim808Deal {
  uint8_t sim808_step;
  uint8_t answer_flag;
  uint8_t authen_flag;
  uint16_t authen_len;
  uint8_t authen_buf[16];
};

#pragma pack(pop)

#endif // JT808_SERVICE_JT808_PROTOCOL_H_
