#ifndef JT808_COMMON_JT808_TERMINAL_PARAMETERS_H_
#define JT808_COMMON_JT808_TERMINAL_PARAMETERS_H_

#include <stdint.h>

#include <string>
#include <list>
#include <vector>


// DWORD 终端心跳发送间隔(s)
#define HEARTBEATINTERVAL     0x0001

// DWORD TCP消息应答超时时间(s)
#define TCPRESPONDTIMEOUT     0x0002

// DWORD TCP消息重传次数
#define TCPMSGRETRANSTIMES    0x0003

// DWORD UDP消息应答超时时间(s)
#define UDPRESPONDTIMEOUT     0x0004

// DWORD UDP消息重传次数
#define UDPMSGRETRANSTIMES    0x0005

// DWORD SMS消息应答超时时间(s)
#define SMSRESPONDTIMEOUT     0x0006

// DWORD SMS消息重传次数
#define SMSMSGRETRANSTIMES    0x0007

// DWORD 位置汇报策略, 0:定时汇报; 1:定距汇报; 2:定时和定距
//       汇报.
#define POSITIONREPORTWAY     0x0020

// DWORD 位置汇报方案, 0:根据 ACC 状态; 1:根据登录状态和ACC
//       状态, 先判断登录状态, 若登录再根据 ACC 状态.
#define POSITIONREPORTPLAN    0x0021

// DWORD 驾驶员未登录汇报时间间隔
#define NOTLOGINREPORTTIMEINTERVAL   0x0022

// DWORD 休眠时汇报时间间隔
#define SLEEPREPORTTIMEINTERVAL      0x0027

// DWORD 紧急报警时汇报时间间隔
#define ALARMREPORTTIMEINTERVAL      0x0028

// DWORD 缺省时间汇报间隔
#define DEFTIMEREPORTTIMEINTERVAL    0x0029

// DWORD 缺省距离汇报间隔
#define DEFTIMEREPORTDISTANCEINTERVAL    0x002C

// DWORD 驾驶员未登录汇报距离间隔
#define NOTLOGINREPORTDISTANCEINTERVAL   0x002D

// DWORD 休眠时汇报距离间隔
#define SLEEPREPORTDISTANCEINTERVAL      0x002E

// DWORD 紧急报警时汇报距离间隔
#define ALARMREPORTDISTANCEINTERVAL      0x002F

// DWORD 拐点补传角度, < 180
#define INFLECTIONPOINTRETRANSANGLE      0x0030


// DWORD 报警屏蔽字, 与位置信息汇报消息中的报警标志相对
//       应, 相应位为1则相应报警被屏蔽
#define ALARMSHIELDWORD       0x0050

// DWORD 报警发送文本SMS开关, 与位置信息汇报消息中的报警
//       标志相对应, 相应位为1则相应报警时发送文本SMS
#define ALARMSENDTXT          0x0051

// DWORD 报警拍摄开关, 与位置信息汇报消息中的报警标志相
//       对应, 相应位为1则相应报警时摄像头拍摄
#define ALARMSHOOTSWITCH      0x0052

// DWORD 报警拍摄存储标志, 与位置信息汇报消息中的报警标
//       志相对应, 相应位为1则对相应报警时拍的照片进行
//       存储, 否则实时上传
#define ALARMSHOOTSAVEFLAGS   0x0053

// DWORD 关键标志, 与位置信息汇报消息中的报警标志相对应,
//       相应位为1则对相应报警为关键报警
#define ALARMKEYFLAGS         0x0054

// DWORD 最高速度, km/h
#define MAXSPEED              0x0055

// BYTE  GNSS定位模式, 定义如下:
//       bit0, 0: 禁用GPS定位, 1: 启用GPS定位;
//       bit1, 0: 禁用北斗定位, 1: 启用北斗定位;
//       bit2, 0: 禁用GLONASS定位, 1: 启用GLONASS定位;
//       bit3, 0: 禁用Galileo定位, 1: 启用Galileo定位.
#define GNSSPOSITIONMODE      0x0090

// BYTE  GNSS波特率, 定义如下:
//       0x00: 4800; 0x01: 9600: 0x02: 19200;
//       0x03: 38400; 0x04: 57600; 0x05: 115200.
#define GNSSBAUDERATE         0x0091

// BYTE  GNSS模块详细定位数据输出频率, 定义如下:
//       0x00: 500ms; 0x01: 1000ms(默认值);
//       0x02: 2000ms; 0x03: 3000ms; 0x04: 4000m.
#define GNSSOUTPUTFREQ        0x0092

// DWORD GNSS模块详细定位数据采集频率(s), 默认为1.
#define GNSSOUTPUTCOLLECTFREQ            0x0093

// BYTE  GNSS模块详细定位数据上传方式:
//       0x00, 本地存储, 不上传(默认值);
//       0x01, 按时间间隔上传;
//       0x02, 按距离间隔上传;
//       0x0B, 按累计时间上传, 达到传输时间后自动停止上传;
//       0x0C, 按累计距离上传, 达到距离后自动停止上传;
//       0x0D, 按累计条数上传, 达到上传条数后自动停止上传.
#define GNSSUPLOADWAY         0x0094

// DWORD GNSS模块详细定位数据上传设置:
//       上传方式为0x01时, 单位为秒;
//       上传方式为0x02时, 单位为米;
//       上传方式为0x0B时, 单位为秒;
//       上传方式为0x0C时, 单位为米;
//       上传方式为0x0D时, 单位为条.
#define GNSSUPLOADSET         0x0095

// DWORD   CAN总线通道1采集时间间隔(ms), 0表示不采集
#define CAN1COLLECTINTERVAL   0x0100

// WORD    CAN总线通道1上传时间间隔(s), 0表示不上传
#define CAN1UPLOADINTERVAL    0x0101

// DWORD   CAN总线通道2采集时间间隔(ms), 0表示不采集
#define CAN2COLLECTINTERVAL   0x0102

// WORD    CAN总线通道2上传时间间隔(s), 0表示不上传
#define CAN2UPLOADINTERVAL    0x0103

// BYTE[8] CAN总线ID单独采集设置:
//         bit63-bit32 表示此ID采集时间间隔(ms), 0表示不采集;
//         bit31 表示CAN通道号, 0: CAN1, 1: CAN2;
//         bit30 表示帧类型, 0: 标准帧, 1: 扩展帧;
//         bit29 表示数据采集方式, 0: 原始数据, 1: 采集区间的计算值;
//         bit28-bit0 表示CAN总线ID.
#define CANSPECIALSET         0x0110

// 用户自定义

// 启用模块
// BYTE    启用GPS
#define STARTUPGPS            0xF000
// BYTE    启用CDRadio
#define STARTUPCDRADIO        0xF001
// BYTE    启用NTRIP差分站
#define STARTUPNTRIPCORS      0xF002
// BYTE    启用NTRIP后台
#define STARTUPNTRIPSERV      0xF003
// BYTE    启用JT808后台
#define STARTUPJT808SERV      0xF004

// GPS模块
// BYTE    输出GGA格式数据
#define GPSLOGGGA             0xF010
// BYTE    输出RMC格式数据
#define GPSLOGRMC             0xF011
// BYTE    输出ATT格式数据
#define GPSLOGATT             0xF012

// CDRadio模块
// DWORD   输出波特率
#define CDRADIOBAUDERATE      0xF020
// WORD    工作频点
#define CDRADIOWORKINGFREQ    0xF021
// BYTE    接收模式
#define CDRADIORECEIVEMODE    0xF022
// BYTE    业务编号
#define CDRADIOFORMCODE       0xF023

// NTRIP 差分站
// STRING  地址
#define NTRIPCORSIP                      0xF030
// WORD    端口
#define NTRIPCORSPORT                    0xF031
// STRING  用户名
#define NTRIPCORSUSERNAME                0xF032
// STRING  密码
#define NTRIPCORSPASSWD                  0xF033
// STRING  挂载点
#define NTRIPCORSMOUNTPOINT              0xF034
// BYTE    GGA汇报间隔
#define NTRIPCORSREPORTINTERVAL          0xF035

// NTRIP 后台
// STRING  地址
#define NTRIPSERVICEIP                   0xF040
// WORD    端口
#define NTRIPSERVICEPORT                 0xF041
// STRING  用户名
#define NTRIPSERVICEUSERNAME             0xF042
// STRING  密码
#define NTRIPSERVICEPASSWD               0xF043
// STRING  挂载点
#define NTRIPSERVICEMOUNTPOINT           0xF044
// BYTE    GGA汇报间隔
#define NTRIPSERVICEREPORTINTERVAL       0xF045

// JT808 后台
// STRING  地址
#define JT808SERVICEIP                   0xF050
// WORD    端口
#define JT808SERVICEPORT                 0xF051
// STRING  终端手机号
#define JT808SERVICEPHONENUM             0xF052
// BYTE    汇报间隔
#define JT808SERVICEREPORTINTERVAL       0xF053


#define MAX_TERMINAL_PARAMETER_LEN_A_RECORD     (1023 - 3)

// 参数数据类型
enum ParametersType {
  kUnknowType = 0x0,
  kByteType,
  kWordType,
  kDwordType,
  kStringType,
};

uint8_t GetParameterTypeByParameterId(const uint32_t &para_id);
uint16_t GetParameterLengthByParameterType(const uint8_t &para_type);


#endif // JT808_COMMON_JT808_TERMINAL_PARAMETERS_H_
