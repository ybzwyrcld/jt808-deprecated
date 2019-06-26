// Copyright 2019 Yuming Meng. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "terminal/jt808_area_route.h"

#include <sys/types.h>

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <string>

#include "bcd/bcd.h"
#include "common/jt808_util.h"
#include "util/container_clear.h"


void ClearAreaRouteListElement(AreaRouteSet *area_route_set) {
  if (area_route_set->circular_area_list != nullptr) {
    ClearContainerElement(area_route_set->circular_area_list);
  }
  if (area_route_set->rectangle_area_list != nullptr) {
    ClearContainerElement(area_route_set->rectangle_area_list);
  }
  if (area_route_set->polygonal_area_list != nullptr) {
    for (auto &polygonal_area : *area_route_set->polygonal_area_list) {
      ClearContainerElement(polygonal_area->coordinate_list);
      delete polygonal_area->coordinate_list;
    }
    ClearContainerElement(area_route_set->polygonal_area_list);
  }
  if (area_route_set->route_list != nullptr) {
    for (auto &route : *area_route_set->route_list) {
      ClearContainerElement(route->inflection_point_list);
      delete route->inflection_point_list;
    }
    ClearContainerElement(area_route_set->route_list);
  }
}

static int PreparaCircularAreaList(
               const std::vector<std::string> &para_list,
               std::list<CircularArea *> *circular_area_list) {
  uint32_t u32val;
  char time[6];
  std::string str;

  if (circular_area_list == nullptr) {
    circular_area_list = new std::list<CircularArea *>;
  }
  auto para_it = para_list.begin();
  para_it++;
  CircularArea *circular_area = new CircularArea;
  sscanf(para_it->c_str(), "%x", &u32val);
  circular_area->area_id = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%x", &u32val);
  circular_area->area_attribute.value = static_cast<uint16_t>(u32val);
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  circular_area->center_point.latitude = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  circular_area->center_point.longitude = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  circular_area->radius = u32val;
  ++para_it;
  if (circular_area->area_attribute.bit.bytime) {
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(circular_area->start_time, time, 6);
    ++para_it;
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(circular_area->end_time, time, 6);
    ++para_it;
  }
  if (circular_area->area_attribute.bit.speedlimit) {
    sscanf(para_it->c_str(), "%u", &u32val);
    circular_area->max_speed = static_cast<uint16_t>(u32val);
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    circular_area->overspeed_duration = static_cast<uint8_t>(u32val);
    ++para_it;
  }
  circular_area_list->push_back(circular_area);

  return (para_it == para_list.end() ? 0 : -1);
}

static int PreparaRectangleAreaList(
               const std::vector<std::string> &para_list,
               std::list<RectangleArea *> *rectangle_area_list) {
  uint32_t u32val;
  char time[6];
  std::string str;

  if (rectangle_area_list == nullptr) {
    rectangle_area_list = new std::list<RectangleArea *>;
  }
  auto para_it = para_list.begin();
  para_it++;
  RectangleArea *rectangle_area = new RectangleArea;
  sscanf(para_it->c_str(), "%x", &u32val);
  rectangle_area->area_id = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%x", &u32val);
  rectangle_area->area_attribute.value = static_cast<uint16_t>(u32val);
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  rectangle_area->upper_left_corner.latitude = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  rectangle_area->upper_left_corner.longitude = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  rectangle_area->bottom_right_corner.latitude = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%u", &u32val);
  rectangle_area->bottom_right_corner.longitude = u32val;
  ++para_it;
  if (rectangle_area->area_attribute.bit.bytime) {
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(rectangle_area->start_time, time, 6);
    ++para_it;
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(rectangle_area->end_time, time, 6);
    ++para_it;
  }
  if (rectangle_area->area_attribute.bit.speedlimit) {
    sscanf(para_it->c_str(), "%u", &u32val);
    rectangle_area->max_speed = static_cast<uint16_t>(u32val);
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    rectangle_area->overspeed_duration = static_cast<uint8_t>(u32val);
    ++para_it;
  }
  rectangle_area_list->push_back(rectangle_area);

  return (para_it == para_list.end() ? 0 : -1);
}

static int PreparaPolygonalAreaList(
               const std::vector<std::string> &para_list,
               std::list<PolygonalArea *> *polygonal_area_list) {
  uint32_t u32val;
  char time[6];
  std::string str;

  if (polygonal_area_list == nullptr) {
    polygonal_area_list = new std::list<PolygonalArea *>;
  }
  auto para_it = para_list.begin();
  ++para_it;
  PolygonalArea *polygonal_area = new PolygonalArea;
  sscanf(para_it->c_str(), "%x", &u32val);
  polygonal_area->area_id = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%x", &u32val);
  polygonal_area->area_attribute.value = static_cast<uint16_t>(u32val);
  ++para_it;
  if (polygonal_area->area_attribute.bit.bytime) {
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(polygonal_area->start_time, time, 6);
    ++para_it;
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(polygonal_area->end_time, time, 6);
    ++para_it;
  }
  if (polygonal_area->area_attribute.bit.speedlimit) {
    sscanf(para_it->c_str(), "%u", &u32val);
    polygonal_area->max_speed = static_cast<uint16_t>(u32val);
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    polygonal_area->overspeed_duration = static_cast<uint8_t>(u32val);
    ++para_it;
  }
  sscanf(para_it->c_str(), "%u", &u32val);
  polygonal_area->coordinate_count = static_cast<uint16_t>(u32val);
  ++para_it;
  Coordinate *coordinate;
  polygonal_area->coordinate_list = new std::vector<Coordinate *>;
  for (int i = 0; i < polygonal_area->coordinate_count; ++i) {
    coordinate = new Coordinate;
    sscanf(para_it->c_str(), "%u", &u32val);
    coordinate->latitude = u32val;
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    coordinate->longitude = u32val;
    ++para_it;
    polygonal_area->coordinate_list->push_back(coordinate);
  }
  polygonal_area_list->push_back(polygonal_area);

  return (para_it == para_list.end() ? 0 : -1);
}

static int PreparaRouteList(
               const std::vector<std::string> &para_list,
               std::list<Route *> *route_list) {
  uint32_t u32val;
  char time[6];
  std::string str;

  if (route_list == nullptr) {
    route_list = new std::list<Route *>;
  }
  auto para_it = para_list.begin();
  ++para_it;
  Route *route = new Route;
  sscanf(para_it->c_str(), "%x", &u32val);
  route->route_id = u32val;
  ++para_it;
  sscanf(para_it->c_str(), "%x", &u32val);
  route->route_attribute.value = static_cast<uint16_t>(u32val);
  ++para_it;
  if (route->route_attribute.bit.bytime) {
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(route->start_time, time, 6);
    ++para_it;
    BcdFromStringCompress(para_it->c_str(), time, para_it->size());
    memcpy(route->end_time, time, 6);
    ++para_it;
  }
  sscanf(para_it->c_str(), "%u", &u32val);
  route->inflection_point_count = static_cast<uint16_t>(u32val);
  ++para_it;
  InflectionPoint *inflection_point;
  route->inflection_point_list = new std::vector<InflectionPoint *>;
  for (int i = 0; i < route->inflection_point_count; ++i) {
    inflection_point = new InflectionPoint;
    sscanf(para_it->c_str(), "%x", &u32val);
    inflection_point->inflection_point_id = u32val;
    ++para_it;
    sscanf(para_it->c_str(), "%x", &u32val);
    inflection_point->road_section_id = u32val;
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    inflection_point->coordinate.latitude = u32val;
    ++para_it;
    sscanf(para_it->c_str(), "%u", &u32val);
    inflection_point->coordinate.longitude = u32val;
    ++para_it;
    sscanf(para_it->c_str(), "%x", &u32val);
    inflection_point->road_section_wide = static_cast<uint8_t>(u32val);
    ++para_it;
    sscanf(para_it->c_str(), "%x", &u32val);
    inflection_point->road_section_attribute.value =
        static_cast<uint8_t>(u32val);
    ++para_it;
    if (inflection_point->road_section_attribute.bit.traveltime) {
      sscanf(para_it->c_str(), "%x", &u32val);
      inflection_point->max_driving_time = static_cast<uint16_t>(u32val);
      ++para_it;
      sscanf(para_it->c_str(), "%x", &u32val);
      inflection_point->min_driving_time = static_cast<uint16_t>(u32val);
      ++para_it;
    }
    if (inflection_point->road_section_attribute.bit.traveltime) {
      sscanf(para_it->c_str(), "%u", &u32val);
      inflection_point->max_speed = static_cast<uint16_t>(u32val);
      ++para_it;
      sscanf(para_it->c_str(), "%u", &u32val);
      inflection_point->overspeed_duration = static_cast<uint8_t>(u32val);
      ++para_it;
    }
    route->inflection_point_list->push_back(inflection_point);
  }
  route_list->push_back(route);

  return (para_it == para_list.end() ? 0 : -1);
}

void ReadAreaRouteFormFile(const char *path, AreaRouteSet *area_route_set) {
  char *result;
  uint32_t u32val;
  char line[1024] = {0};

  system("sync");
  system("sync");
  std::ifstream ifs;
  ifs.open(path, std::ios::in | std::ios::binary);
  if (ifs.is_open()) {
    std::string str;
    std::vector<std::string> para_item_list;
    char *remaining;
    while (getline(ifs, str)) {
      memset(line, 0x0, sizeof(line));
      str.copy(line, str.length(), 0);
      result = strtok_r(line, ";", &remaining);
      para_item_list.clear();
      while (result != nullptr) {
        std::string para_item(result);
        para_item_list.push_back(para_item);
        result = strtok_r(nullptr, ";", &remaining);
      }
      str = para_item_list.front();
      sscanf(str.c_str(), "%u", &u32val);
      if (u32val == kCircular) {
        PreparaCircularAreaList(para_item_list,
                                area_route_set->circular_area_list);
      } else if (u32val == kRectangle) {
        PreparaRectangleAreaList(para_item_list,
                                 area_route_set->rectangle_area_list);
      } else if (u32val == kPolygonal) {
        PreparaPolygonalAreaList(para_item_list,
                                 area_route_set->polygonal_area_list);
      } else if (u32val == kRoute) {
        PreparaRouteList(para_item_list, area_route_set->route_list);
      }
    }
    ifs.close();
  }
}

static void GenerateBufferLineByCircularArea(const CircularArea &circular_area,
                                             std::string *buffer) {
  char time[6] = {0};
  char line[256] = {0};
  buffer->clear();
  snprintf(line, sizeof(line), "1;%x;%x;%u;%u;%u;",
      circular_area.area_id,
      circular_area.area_attribute.value,
      circular_area.center_point.latitude,
      circular_area.center_point.longitude,
      circular_area.radius);
  *buffer += line;
  if (circular_area.area_attribute.bit.bytime) {
    memset(line, 0x0, sizeof(line));
    memcpy(time, circular_area.start_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
    memset(line, 0x0, sizeof(line));
    memcpy(time, circular_area.end_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
  }
  if (circular_area.area_attribute.bit.speedlimit) {
    memset(line, 0x0, sizeof(line));
    snprintf(line, sizeof(line), "%u;%u;", circular_area.max_speed,
             circular_area.overspeed_duration);
    *buffer += line;
  }
  *buffer += "\n";
}

static void GenerateBufferLineByRectangleArea(
                const RectangleArea &rectangle_area,
                std::string *buffer) {
  char time[6] = {0};
  char line[256] = {0};
  buffer->clear();
  snprintf(line, sizeof(line), "2;%x;%x;%u;%u;%u;%u;",
      rectangle_area.area_id,
      rectangle_area.area_attribute.value,
      rectangle_area.upper_left_corner.latitude,
      rectangle_area.upper_left_corner.longitude,
      rectangle_area.bottom_right_corner.latitude,
      rectangle_area.bottom_right_corner.longitude);
  *buffer += line;
  if (rectangle_area.area_attribute.bit.bytime) {
    memset(line, 0x0, sizeof(line));
    memcpy(time, rectangle_area.start_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
    memset(line, 0x0, sizeof(line));
    memcpy(time, rectangle_area.end_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
  }
  if (rectangle_area.area_attribute.bit.speedlimit) {
    memset(line, 0x0, sizeof(line));
    snprintf(line, sizeof(line), "%u;%u;", rectangle_area.max_speed,
             rectangle_area.overspeed_duration);
    *buffer += line;
  }
  *buffer += "\n";
}

static void GenerateBufferLineByPolygonalArea(
                const PolygonalArea &polygonal_area,
                std::string *buffer) {
  char time[6] = {0};
  char line[256] = {0};
  buffer->clear();
  snprintf(line, sizeof(line), "3;%x;%x;",
      polygonal_area.area_id,
      polygonal_area.area_attribute.value);
  *buffer += line;
  if (polygonal_area.area_attribute.bit.bytime) {
    memset(line, 0x0, sizeof(line));
    memcpy(time, polygonal_area.start_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
    memset(line, 0x0, sizeof(line));
    memcpy(time, polygonal_area.end_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
  }
  if (polygonal_area.area_attribute.bit.speedlimit) {
    memset(line, 0x0, sizeof(line));
    snprintf(line, sizeof(line), "%u;%u;", polygonal_area.max_speed,
             polygonal_area.overspeed_duration);
    *buffer += line;
  }
  *buffer += std::to_string(polygonal_area.coordinate_count) + ";";
  if (polygonal_area.coordinate_count) {
    auto coordinate_it = polygonal_area.coordinate_list->begin();
    while (coordinate_it != polygonal_area.coordinate_list->end()) {
      *buffer += std::to_string((*coordinate_it)->latitude) + ";";
      *buffer += std::to_string((*coordinate_it)->longitude) + ";";
      ++coordinate_it;
    }
  }
  *buffer += "\n";
}

static void GenerateBufferLineByRoute(
                const Route &route,
                std::string *buffer) {
  char time[6] = {0};
  char line[256] = {0};
  buffer->clear();
  snprintf(line, sizeof(line), "4;%x;%x;",
      route.route_id,
      route.route_attribute.value);
  *buffer += line;
  if (route.route_attribute.bit.bytime) {
    memset(line, 0x0, sizeof(line));
    memcpy(time, route.start_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
    memset(line, 0x0, sizeof(line));
    memcpy(time, route.end_time, 6);
    *buffer += StringFromBcdCompressFillingZero(time, line, 6);
    *buffer += ";";
  }
  *buffer += std::to_string(route.inflection_point_count) + ";";
  if (route.inflection_point_count) {
    auto point_it = route.inflection_point_list->begin();
    while (point_it != route.inflection_point_list->end()) {
      memset(line, 0x0, sizeof(line));
      snprintf(line, sizeof(line), "%x;%x;%u;%u;%u;%x;",
               (*point_it)->inflection_point_id,
               (*point_it)->road_section_id,
               (*point_it)->coordinate.latitude,
               (*point_it)->coordinate.longitude,
               (*point_it)->road_section_wide,
               (*point_it)->road_section_attribute.value);
      *buffer += line;
      if ((*point_it)->road_section_attribute.bit.traveltime) {
        *buffer += std::to_string((*point_it)->max_driving_time) + ";";
        *buffer += std::to_string((*point_it)->min_driving_time) + ";";
      }
      if ((*point_it)->road_section_attribute.bit.speedlimit) {
        *buffer += std::to_string((*point_it)->max_speed) + ";";
        *buffer += std::to_string((*point_it)->overspeed_duration) + ";";
      }
      ++point_it;
    }
  }
  *buffer += "\n";
}

void WriteAreaRouteToFile(const char *path,
                          const AreaRouteSet &area_route_set) {
  std::ofstream ofs;
  ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (ofs.is_open()) {
    std::string str;
    if (area_route_set.circular_area_list != nullptr) {
      for (auto &circular_area : *area_route_set.circular_area_list) {
        GenerateBufferLineByCircularArea(*circular_area, &str);
        ofs.write(str.c_str(), str.size());
      }
    }
    if (area_route_set.rectangle_area_list != nullptr) {
      for (auto &rectangle_area : *area_route_set.rectangle_area_list) {
        GenerateBufferLineByRectangleArea(*rectangle_area, &str);
        ofs.write(str.c_str(), str.size());
      }
    }
    if (area_route_set.polygonal_area_list != nullptr) {
      for (auto &polygonal_area : *area_route_set.polygonal_area_list) {
        GenerateBufferLineByPolygonalArea(*polygonal_area, &str);
        ofs.write(str.c_str(), str.size());
      }
    }
    if (area_route_set.route_list != nullptr) {
      for (auto &route : *area_route_set.route_list) {
        GenerateBufferLineByRoute(*route, &str);
        ofs.write(str.c_str(), str.size());
      }
    }
    ofs.close();
  }
}

int DealSetCircularAreaRequest(const uint8_t *data,
                               AreaRouteSet *area_route_set) {
  uint8_t operation = *data++;
  uint8_t count = *data++;
  uint16_t u16val = 0;
  uint32_t u32val = 0;
  for (int i = 0; i < count; ++i) {
    CircularArea *circular_area = new CircularArea;
    memcpy(&u32val, data, 4);
    circular_area->area_id = EndianSwap32(u32val);
    data += 4;
    memcpy(&u16val, data, 2);
    circular_area->area_attribute.value = EndianSwap16(u16val);
    data += 2;
    memcpy(&u32val, data, 4);
    circular_area->center_point.latitude = EndianSwap32(u32val);
    data += 4;
    memcpy(&u32val, data, 4);
    circular_area->center_point.longitude = EndianSwap32(u32val);
    data += 4;
    memcpy(&u32val, data, 4);
    circular_area->radius = EndianSwap32(u32val);
    data += 4;
    if (circular_area->area_attribute.bit.bytime) {
      memcpy(circular_area->start_time, data, 6);
      data += 6;
      memcpy(circular_area->end_time, data, 6);
      data += 6;
    }
    if (circular_area->area_attribute.bit.speedlimit) {
      memcpy(&u16val, data, 2);
      circular_area->max_speed = EndianSwap16(u16val);
      data += 2;
      circular_area->overspeed_duration = *data++;
    }
    if (area_route_set->circular_area_list == nullptr) {
      area_route_set->circular_area_list = new std::list<CircularArea *>;
    }
    if (operation == kAppendArea) {
      area_route_set->circular_area_list->push_back(circular_area);
    } else {
      auto area_it = area_route_set->circular_area_list->begin();
      while (area_it != area_route_set->circular_area_list->end()) {
        if ((*area_it)->area_id == circular_area->area_id) {
          delete *area_it;
          area_route_set->circular_area_list->erase(area_it);
          area_route_set->circular_area_list->push_back(circular_area);
          break;
        } else {
          ++area_it;
        }
      }
    }
  }
  return (count > 0 ? 0 : -1);
}

int DealSetRectangleAreaRequest(const uint8_t *data,
                                AreaRouteSet *area_route_set) {
  uint8_t operation = *data++;
  uint8_t count = *data++;
  uint16_t u16val = 0;
  uint32_t u32val = 0;
  for (int i = 0; i < count; ++i) {
    RectangleArea *rectangle_area = new RectangleArea;
    memcpy(&u32val, data, 4);
    rectangle_area->area_id = EndianSwap32(u32val);
    data += 4;
    memcpy(&u16val, data, 2);
    rectangle_area->area_attribute.value = EndianSwap16(u16val);
    data += 2;
    memcpy(&u32val, data, 4);
    rectangle_area->upper_left_corner.latitude = EndianSwap32(u32val);
    data += 4;
    memcpy(&u32val, data, 4);
    rectangle_area->upper_left_corner.longitude = EndianSwap32(u32val);
    data += 4;
    memcpy(&u32val, data, 4);
    rectangle_area->bottom_right_corner.latitude = EndianSwap32(u32val);
    data += 4;
    memcpy(&u32val, data, 4);
    rectangle_area->bottom_right_corner.longitude = EndianSwap32(u32val);
    data += 4;
    if (rectangle_area->area_attribute.bit.bytime) {
      memcpy(rectangle_area->start_time, data, 6);
      data += 6;
      memcpy(rectangle_area->end_time, data, 6);
      data += 6;
    }
    if (rectangle_area->area_attribute.bit.speedlimit) {
      memcpy(&u16val, data, 2);
      rectangle_area->max_speed = EndianSwap16(u16val);
      data += 2;
      rectangle_area->overspeed_duration = *data++;
    }
    if (area_route_set->rectangle_area_list == nullptr) {
      area_route_set->rectangle_area_list = new std::list<RectangleArea *>;
    }
    if (operation == kAppendArea) {
      area_route_set->rectangle_area_list->push_back(rectangle_area);
    } else {
      auto area_it = area_route_set->rectangle_area_list->begin();
      while (area_it != area_route_set->rectangle_area_list->end()) {
        if ((*area_it)->area_id == rectangle_area->area_id) {
          delete *area_it;
          area_route_set->rectangle_area_list->erase(area_it);
          area_route_set->rectangle_area_list->push_back(rectangle_area);
          break;
        } else {
          ++area_it;
        }
      }
    }
  }
  return (count > 0 ? 0 : -1);
}

int DealSetPolygonalAreaRequest(const uint8_t *data,
                                AreaRouteSet *area_route_set) {
  uint8_t operation = *data++;
  uint8_t count = *data++;
  uint16_t u16val = 0;
  uint32_t u32val = 0;
  operation = *data++;
  count = *data++;
  for (int i = 0; i < count; ++i) {
    PolygonalArea *polygonal_area = new PolygonalArea;
    memcpy(&u32val, data, 4);
    polygonal_area->area_id = EndianSwap32(u32val);
    data += 4;
    memcpy(&u16val, data, 2);
    polygonal_area->area_attribute.value = EndianSwap16(u16val);
    data += 2;
    if (polygonal_area->area_attribute.bit.bytime) {
      memcpy(polygonal_area->start_time, data, 6);
      data += 6;
      memcpy(polygonal_area->end_time, data, 6);
      data += 6;
    }
    if (polygonal_area->area_attribute.bit.speedlimit) {
      memcpy(&u16val, data, 2);
      polygonal_area->max_speed = EndianSwap16(u16val);
      data += 2;
      polygonal_area->overspeed_duration = *data++;
    }
    memcpy(&u16val, data, 2);
    polygonal_area->coordinate_count = EndianSwap16(u16val);
    data += 2;
    polygonal_area->coordinate_list = new std::vector<Coordinate*>;
    for (int i = 0; i < polygonal_area->coordinate_count; ++i) {
      Coordinate *coordinate = new Coordinate;
      memcpy(&u32val, data, 4);
      coordinate->latitude = EndianSwap32(u32val);
      data += 4;
      memcpy(&u32val, data, 4);
      coordinate->longitude = EndianSwap32(u32val);
      data += 4;
      polygonal_area->coordinate_list->push_back(coordinate);
    }
    if (area_route_set->polygonal_area_list == nullptr) {
      area_route_set->polygonal_area_list = new std::list<PolygonalArea *>;
    }
    if (operation == kAppendArea) {
      area_route_set->polygonal_area_list->push_back(polygonal_area);
    } else {
      auto area_it = area_route_set->polygonal_area_list->begin();
      while (area_it != area_route_set->polygonal_area_list->end()) {
        if ((*area_it)->area_id == polygonal_area->area_id) {
          ClearContainerElement((*area_it)->coordinate_list);
          delete (*area_it)->coordinate_list;
          delete *area_it;
          area_route_set->polygonal_area_list->erase(area_it);
          area_route_set->polygonal_area_list->push_back(polygonal_area);
          break;
        } else {
          ++area_it;
        }
      }
    }
  }
  return (count > 0 ? 0 : -1);
}

int DealSetRouteAreaRequest(const uint8_t *data,
                            AreaRouteSet *area_route_set) {
  uint8_t operation = *data++;
  uint8_t count = *data++;
  uint16_t u16val = 0;
  uint32_t u32val = 0;
  operation = *data++;
  count = *data++;
  for (int i = 0; i < count; ++i) {
    Route *route = new Route;
    memcpy(&u32val, data, 4);
    route->route_id = EndianSwap32(u32val);
    data += 4;
    memcpy(&u16val, data, 2);
    route->route_attribute.value = EndianSwap16(u16val);
    data += 2;
    if (route->route_attribute.bit.bytime) {
      memcpy(route->start_time, data, 6);
      data += 6;
      memcpy(route->end_time, data, 6);
      data += 6;
    }
    memcpy(&u16val, data, 2);
    route->inflection_point_count = EndianSwap16(u16val);
    data += 2;
    route->inflection_point_list = new std::vector<InflectionPoint *>;
    for (int i = 0; i < route->inflection_point_count; ++i) {
      InflectionPoint *inflection_point = new InflectionPoint;
      memcpy(&u32val, data, 4);
      inflection_point->inflection_point_id = EndianSwap32(u32val);
      data += 4;
      memcpy(&u32val, data, 4);
      inflection_point->road_section_id = EndianSwap32(u32val);
      data += 4;
      memcpy(&u32val, data, 4);
      inflection_point->coordinate.latitude = EndianSwap32(u32val);
      data += 4;
      memcpy(&u32val, data, 4);
      inflection_point->coordinate.longitude = EndianSwap32(u32val);
      data += 4;
      inflection_point->road_section_wide = *data++;
      inflection_point->road_section_attribute.value = *data++;
      if (inflection_point->road_section_attribute.bit.traveltime) {
        memcpy(&u16val, data, 2);
        inflection_point->max_driving_time = EndianSwap16(u16val);
        data += 2;
        memcpy(&u16val, data, 2);
        inflection_point->min_driving_time = EndianSwap16(u16val);
        data += 2;
      }
      if (inflection_point->road_section_attribute.bit.speedlimit) {
        memcpy(&u16val, data, 2);
        inflection_point->max_speed = EndianSwap16(u16val);
        data += 2;
        inflection_point->overspeed_duration = *data++;
      }
      route->inflection_point_list->push_back(inflection_point);
    }
    if (area_route_set->route_list == nullptr) {
      area_route_set->route_list = new std::list<Route *>;
    }
    if (operation == kAppendArea) {
      area_route_set->route_list->push_back(route);
    } else {
      auto route_it = area_route_set->route_list->begin();
      while (route_it != area_route_set->route_list->end()) {
        if ((*route_it)->route_id == route->route_id) {
          ClearContainerElement((*route_it)->inflection_point_list);
          delete (*route_it)->inflection_point_list;
          delete *route_it;
          area_route_set->route_list->erase(route_it);
          area_route_set->route_list->push_back(route);
          break;
        } else {
          ++route_it;
        }
      }
    }
  }
  return (count > 0 ? 0 : -1);
}

int DeleteAreaRouteFromSet(const uint8_t *data, const uint8_t &type,
                           AreaRouteSet *area_route_set) {
  uint8_t count = *data++;
  int retval = 0;
  uint32_t u32val = 0;
  switch (type) {
    case kCircular:
      if (count == 0) {
        ClearContainerElement(area_route_set->circular_area_list);
      } else {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, data + 4*i, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set->circular_area_list->begin();
          while (area_it != area_route_set->circular_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              delete *area_it;
              area_it = area_route_set->circular_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (area_route_set->circular_area_list->empty()) {
        delete area_route_set->circular_area_list;
        area_route_set->circular_area_list = nullptr;
      }
      break;
    case kRectangle:
      if (count == 0) {
        ClearContainerElement(area_route_set->rectangle_area_list);
      } else if (area_route_set->rectangle_area_list != nullptr) {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, data + 4*i, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set->rectangle_area_list->begin();
          while (area_it != area_route_set->rectangle_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              delete *area_it;
              area_route_set->rectangle_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (area_route_set->rectangle_area_list->empty()) {
        delete area_route_set->rectangle_area_list;
        area_route_set->rectangle_area_list = nullptr;
      }
      break;
    case kPolygonal:
      if (count == 0) {
        if (area_route_set->polygonal_area_list != nullptr) {
          auto area_it = area_route_set->polygonal_area_list->begin();
          while (area_it != area_route_set->polygonal_area_list->end()) {
            ClearContainerElement((*area_it)->coordinate_list);
            delete (*area_it)->coordinate_list;
            ++area_it;
          }
        }
        ClearContainerElement(area_route_set->polygonal_area_list);
      } else if (area_route_set->polygonal_area_list != nullptr) {
        uint32_t area_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, data + 4*i, 4);
          area_id = EndianSwap32(u32val);
          auto area_it = area_route_set->polygonal_area_list->begin();
          while (area_it != area_route_set->polygonal_area_list->end()) {
            if ((*area_it)->area_id == area_id) {
              ClearContainerElement((*area_it)->coordinate_list);
              delete (*area_it)->coordinate_list;
              delete *area_it;
              area_route_set->polygonal_area_list->erase(area_it);
              break;
            } else {
              ++area_it;
            }
          }
        }
      }
      if (area_route_set->polygonal_area_list->empty()) {
        delete area_route_set->polygonal_area_list;
        area_route_set->polygonal_area_list = nullptr;
      }
      break;
    case kRoute:
      if (count == 0) {
        if (area_route_set->route_list != nullptr) {
          auto route_it = area_route_set->route_list->begin();
          while (route_it != area_route_set->route_list->end()) {
            ClearContainerElement((*route_it)->inflection_point_list);
            delete (*route_it)->inflection_point_list;
            ++route_it;
          }
        }
        ClearContainerElement(area_route_set->route_list);
      } else if (area_route_set->route_list != nullptr) {
        uint32_t route_id;
        for (int i = 0; i < count; ++i) {
          memcpy(&u32val, data + 4*i, 4);
          route_id = EndianSwap32(u32val);
          auto route_it = area_route_set->route_list->begin();
          while (route_it != area_route_set->route_list->end()) {
            if ((*route_it)->route_id == route_id) {
              ClearContainerElement((*route_it)->inflection_point_list);
              delete (*route_it)->inflection_point_list;
              delete *route_it;
              area_route_set->route_list->erase(route_it);
              break;
            } else {
              ++route_it;
            }
          }
        }
      }
      if (area_route_set->route_list->empty()) {
        delete area_route_set->route_list;
        area_route_set->route_list = nullptr;
      }
      break;
    default:
      retval = -1;
      break;
  }
  return retval;
}
