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

void ClearAreaRouteListElement(AreaRouteSet &area_route_set);
void ReadAreaRouteFormFile(const char *path, AreaRouteSet &area_route_set);
void WriteAreaRouteToFile(const char *path, const AreaRouteSet &area_route_set);

#endif // JT808_TERMINAL_JT808_AREA_ROUTE_H_
