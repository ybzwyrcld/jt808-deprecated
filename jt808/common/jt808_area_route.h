#ifndef JT808_COMMON_JT808_AREA_ROUTE_H_
#define JT808_COMMON_JT808_AREA_ROUTE_H_

#include <stdint.h>

#include <vector>


enum AreaOperation {
  kUpdateArea = 0x0,
  kAppendArea,
  kModefyArea,
};

enum AreaRouteType {
  kCircular = 0x1,
  kRectangle,
  kPolygonal,
  kRoute,
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

union RouteAttribute {
  struct Bit {
    uint16_t bytime:1;  // 1: 根据时间
    uint16_t retain1:1;  // 保留
    uint16_t inalarmtodirver:1;  // 1: 进路线报警给驾驶员
    uint16_t inalarmtoserver:1;  // 1: 进路线报警给平台
    uint16_t outalarmtodirver:1;  // 1: 出路线报警给驾驶员
    uint16_t outalarmtoserver:1;  // 1: 出路线报警给平台
    uint16_t retain2:10;  // 保留
  }bit;
  uint16_t value;
};

union RoadSectionAttribute {
  struct Bit {
    uint8_t traveltime:1;  // 1: 行驶时间
    uint8_t speedlimit:1;  // 1: 限速
    uint8_t snlatitude:1;  // 0: 北纬:1; 1: 南纬
    uint8_t ewlongitude:1;  // 0: 东经:1; 1: 西经
    uint8_t retain:4;  // 保留
  }bit;
  uint8_t value;
};

struct Coordinate {
  uint32_t latitude;  // 纬度
  uint32_t longitude;  // 经度
};

struct InflectionPoint {
  uint32_t inflection_point_id;  // 拐点ID
  uint32_t road_section_id;  // 路段ID
  Coordinate coordinate;  // 拐点经纬度
  uint8_t road_section_wide;  // 路段宽度, 路段为该拐点到下一拐点
  RoadSectionAttribute road_section_attribute;  // 路段属性
  uint16_t max_driving_time;  // 路段行驶过长阈值,
                              // 若路段属性 0 位为 0 则没有该字段
  uint16_t min_driving_time;  // 路段行驶不足阈值,
                              // 若路段属性 0 位为 0 则没有该字段
  uint16_t max_speed;  // 最高速度,
                       // 若路段属性 1 位为 0 则没有该字段
  uint8_t overspeed_duration;  // 超速持续时间,
                               // 若路段属性 1 位为 0 则没有该字段
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

struct Route {
  uint32_t route_id;  // 路线
  RouteAttribute route_attribute;  // 路线属性
  uint8_t start_time[6];  // 起始时间
  uint8_t end_time[6];  // 结束时间
  uint16_t inflection_point_count;  // 路线总拐点数
  std::vector<InflectionPoint *> *inflection_point_list;  // 路线拐点列表
};

#pragma pack(pop)

#endif // JT808_COMMON_JT808_AREA_ROUTE_H_
