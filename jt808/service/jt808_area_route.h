#ifndef JT808_SERVICE_JT808_AREA_ROUTE_H_
#define JT808_SERVICE_JT808_AREA_ROUTE_H_

#include <stdint.h>

#include <vector>


enum AreaOperation {
  kUpdateArea = 0x0,
  kAppendArea,
  kModefyArea,
};

#pragma pack(push, 1)

union AreaAttribute {
  struct Bit {
    uint16_t bytime:1;  // 1: 根据时间
    uint16_t speedlimit:1;  // 1: 限速
    uint16_t inalarmtodirver:1;  // 1: 进区域报警给驾驶员
    uint16_t inalarmtoserver:1;  // 1: 进区域报警给平台
    uint16_t outalarmtodirver:1;  // 1: 出区域报警给驾驶员
    uint16_t outalarmtoserver:1;  // 1: 出区域报警给平台
    uint16_t snlatitude:1;  // 0: 北纬:1; 1: 南纬
    uint16_t ewlongitude:1;  // 0: 东经:1; 1: 西经
    uint16_t canopendoor:1;  // 0: 允许开门:1; 1: 禁止开门
    uint16_t retain:5;  // 保留
    uint16_t inopencommunication:1;  // 0: 进区域开启通信模块:1;
                                   // 1: 进区域关闭通信模块
    uint16_t innotcollectgnss;  // 0: 进区域不采集 GNSS 详细定位
                                  // 数据; 1: 进区域采集 GNSS 详细
                                  // 定位数据
  }bit;
  uint16_t value;
};

struct Coordinate {
  uint32_t latitude;
  uint32_t longitude;
};

struct CircularArea {
  uint32_t area_id;  // 区域ID
  AreaAttribute area_attribute;  // 区域属性
  Coordinate center_point;  // 中心点坐标
  uint32_t radius; // 半径
  uint8_t start_time[6];  // 起始时间
  uint8_t end_time[6];  // 结束时间
  uint16_t max_speed;  // 最高速度
  uint8_t overspeed_duration;  // 超速持续时间
};

struct RectangleArea {
  uint32_t area_id;  // 区域ID
  AreaAttribute area_attribute;  // 区域属性
  Coordinate upper_left_corner;  // 左上点坐标
  Coordinate bottom_right_corner;  // 右下点坐标
  uint8_t start_time[6];  // 起始时间
  uint8_t end_time[6];  // 结束时间
  uint16_t max_speed;  // 最高速度
  uint8_t overspeed_duration;  // 超速持续时间
};

struct PolygonalArea {
  uint32_t area_id;  // 区域ID
  AreaAttribute area_attribute;  // 区域属性
  uint8_t start_time[6];  // 起始时间
  uint8_t end_time[6];  // 结束时间
  uint16_t max_speed;  // 最高速度
  uint8_t overspeed_duration; // 超速持续时间
  uint16_t coordinate_count;  // 坐标总数
  std::vector<Coordinate *> *coordinate_list;  // 坐标列表
};

#pragma pack(pop)

#endif // JT808_SERVICE_JT808_AREA_ROUTE_H_
