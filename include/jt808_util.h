#ifndef __JT808_UTIL_H__
#define __JT808_UTIL_H__

#include <unistd.h>
#include <stdint.h>

#define UP_UNIRESPONSE       0x0001  // 终端通用应答
#define UP_HEARTBEAT         0x0002  // 终端心跳
#define UP_REGISTER          0x0100  // 终端注册
#define UP_LOGOUT            0x0101  // 终端注销
#define UP_AUTHENTICATION    0x0102  // 终端鉴权
#define UP_UPDATERESULT      0x0108  // 终端升级结果
#define UP_POSITIONREPORT    0x0200  // 位置信息上报
#define UP_PASSTHROUGH       0x0900  // 数据上行透传

#define DOWN_UNIRESPONSE        0x8001  // 平台通用应答
#define DOWN_UPDATEPACKAGE      0x8108  // 下发终端升级包
#define DOWN_REGISTERRSPONSE    0x8100  // 终端注册应答
#define DOWN_PASSTHROUGH        0x8900  // 数据下行透传

#define MSGBODY_NOPACKAGE_POS     13
#define MSGBODY_PACKAGE_POS       17
#define PROTOCOL_SIGN             0x7E  // 标识位
#define PROTOCOL_ESCAPE           0x7D  // 转义标识
#define PROTOCOL_ESCAPE_SIGN      0x02  // 0x7e<-->0x7d后紧跟一个0x02
#define PROTOCOL_ESCAPE_ESCAPE    0x01  // 0x7d<-->0x7d后紧跟一个0x01

/*
 * 理论上一条数据的最大长度:
 *       1         + (       16      +     1024      ) * 2 +       1     +       1
 * [PROTOCOL_SIGN] + ( [MSGHEAD LEN] + [MSGBODY LEN] ) * 2 + [CHECK SUM] + [PROTOCOL_SIGN]
 */
#define MAX_PROFRAMEBUF_LEN    4096

#define LOOP_BUFFER_SIZE       5

#pragma pack(1)

struct message_t {
	uint8_t data[MAX_PROFRAMEBUF_LEN];
	uint16_t len;
};

// 终端参数
struct propara_t {
	uint8_t bpara1;
	uint8_t bpara2;
	uint8_t bpara3;
	uint8_t bpara4;
	uint16_t wpara1;
	uint16_t wpara2;
	uint16_t wpara3;
	uint16_t wpara4;
	uint32_t dwpara1;
	uint32_t dwpara2;
	uint8_t strpara1[128];
	uint8_t strpara2[256];
	uint8_t *strpara3;
};


// 消息体属性
union message_body_attr {
	struct bit {
		uint16_t msglen:10;  // 消息体长度
		uint16_t encrypt:3;  // 数据加密方式, 当此三位都为0, 表示消息体不加密; 当第10位为1, 表示消息体经过RSA算法加密
		uint16_t package:1;  // 分包标记
		uint16_t res1:2;  // 保留
	}bit;
	uint16_t val;
};


// 消息头内容
struct message_head {
	uint16_t id;  // 消息ID
	union message_body_attr attribute;  // 消息体属性
	uint8_t phone[6];  // 终端手机号
	uint16_t msgflownum;  // 消息流水号
	uint16_t totalpackage;  // 消息总包数, 该消息分包后的总包数
	uint16_t packetseq;  // 包序号, 从1开始
};


// 报警标志位
union alarm_bit {
	struct bita {
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
		uint32_t res1:5;  // 保留

		uint32_t daydriveovertime:1;  // 当天累计驾驶超时
		uint32_t stopdrivingovertime:1;  // 超时停车
		uint32_t inoutarea:1;  // 进出区域
		uint32_t inoutroad:1;  // 进出路线
		uint32_t roaddrivetime:1;  // 路段行驶时间不足/过长
		uint32_t roaddeviate:1;	 // 路线偏离报警
		uint32_t vssfault:1;  // 车辆VSS故障
		uint32_t oilfault:1;  // 车辆油量异常
		uint32_t caralarm:1;  // 车辆被盗(通过车辆防盗器)
		uint32_t caraccalarm:1;  // 车辆非法点火
		uint32_t carmove:1;  // 车辆非法位移
		uint32_t collision:1;  // 碰撞侧翻报警
		uint32_t res2:2;  // 保留
	}bita;
	uint32_t val;
};

// 状态位
union state_bit {
	struct bits {
		uint32_t acc:1;  // ACC   0:ACC关; 1:ACC开
		uint32_t location:1;  // 定位  0:未定位; 1:定位
		uint32_t snlatitude:1;  // 0:北纬: 1:南纬
		uint32_t ewlongitude:1;  // 0:东经; 1:西经
		uint32_t operation:1;  // 0:运营状态; 1:停运状态
		uint32_t gpsencrypt:1;  // 0:经纬度未经保密插件加密; 1:经纬度已经保密插件加密
		uint32_t trip_stat:2;  // 00:等待新行程; 01:行程开始; 10:正在行驶; 11:行程结束,(有附带数据0x07)
		uint32_t alarm_en:1;  // 防盗功能打开关闭
		uint32_t resetstate:1;  // 上电状态上报

		uint32_t oilcut:1;  // 0:车辆油路正常; 1:车辆油路断开
		uint32_t circuitcut:1;  // 0:车辆电路正常; 1:车辆电路断开
		uint32_t doorlock:1;  // 0:车门解锁; 1: 车门加锁
		uint32_t gpsen:1;  // 1:无GPS数据, 但字段占用; 0:有GPS数据
		uint32_t res2:18;  // 保留
	}bits;
	uint32_t val;
};


// 位置基本信息数据
struct position_basic_info {
	union alarm_bit alarm;
	union state_bit state;
	uint32_t latitude;  // 纬度(以度为单位的纬度值乘以10的6次方, 精确到百万分之一度)
	uint32_t longitude;  // 经度(以度为单位的纬度值乘以10的6次方, 精确到百万分之一度)
	uint16_t atitude;  // 海拔高度, 单位为米(m)
	uint16_t speed;  // 速度 1/10km/h
	uint16_t bearing;  // 方向 0-359,正北为0, 顺时针
	uint8_t time[6];  // 时间 BCD[6] YY-MM-DD-hh-mm-ss(GMT+8时间, 本标准之后涉及的时间均采用此时区)
};


struct register_info {
	uint16_t provinceid;
	uint16_t cityid;
	uint8_t manufacturerid[5];
	uint8_t productmodelid[20];
	uint8_t productid[7];
	uint8_t vehicelcolor;
	uint8_t carlicense[10];
};

enum vichel_color {
	BLUE = 1,
	YELLOW,
	BLACK,
	WHITE,
	OTHER
};

struct sim808_deal {
   uint8_t  sim808_step;
   uint8_t  answer_flag;
   uint8_t  authen_flag;
   uint16_t authen_len;
   uint8_t  authen_buf[16];
};

struct update_package_info {
	uint8_t type_id;
	uint8_t version_len;
	uint8_t version_code[18];
	uint16_t package_total;
	uint16_t packet_seq;
	uint32_t package_len;
	uint8_t *package_ptr;

};

#pragma pack()

#endif // #ifndef __JT808_UTIL_H__
