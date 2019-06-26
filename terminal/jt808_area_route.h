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

#ifndef JT808_TERMINAL_JT808_AREA_ROUTE_H_
#define JT808_TERMINAL_JT808_AREA_ROUTE_H_

#include <stdint.h>

#include <list>
#include <vector>

#include "common/jt808_area_route.h"

struct AreaRouteSet {
  std::list<CircularArea *> *circular_area_list;
  std::list<RectangleArea *> *rectangle_area_list;
  std::list<PolygonalArea *> *polygonal_area_list;
  std::list<Route *> *route_list;
};

void ClearAreaRouteListElement(AreaRouteSet *area_route_set);
void ReadAreaRouteFormFile(const char *path, AreaRouteSet *area_route_set);
void WriteAreaRouteToFile(const char *path, const AreaRouteSet &area_route_set);
int DealSetCircularAreaRequest(const uint8_t *data,
                                AreaRouteSet *area_route_set);
int DealSetRectangleAreaRequest(const uint8_t *data,
                                AreaRouteSet *area_route_set);
int DealSetPolygonalAreaRequest(const uint8_t *data,
                                AreaRouteSet *area_route_set);
int DealSetRouteAreaRequest(const uint8_t *data,
                            AreaRouteSet *area_route_set);
int DeleteAreaRouteFromSet(const uint8_t *data, const uint8_t &type,
                           AreaRouteSet *area_route_set);

#endif  // JT808_TERMINAL_JT808_AREA_ROUTE_H_
