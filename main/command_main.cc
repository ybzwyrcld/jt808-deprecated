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

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <unistd.h>

#include <string>

#include "unix_socket/unix_socket.h"


static inline void PrintUsage(void) {
  printf("Usage: jt808command phonenum [options ...]\n"
         "Options:\n"
         "\tgetterminalparameter [parameterid ...]\n"
         "\tsetterminalparameter [parameterid(HEX):parametervalue ...]\n"
         "\tgetpositioninfo\n"
         "\tpositiontrack reportinterval reportvalidtime\n"
         "\tterminalcontrol controlcommand [controlcommandstring]\n"
         "\tsetcirculararea update/append/modify [circularareaitem ...]\n"
         "\tsetrectanglearea update/append/modify [rectangleareaitem ...]\n"
         "\tsetpolygonalarea update/append/modify [polygonalareaitem ...]\n"
         "\tsetroute update/append/modify [routeitem ...] \n"
         "\tdelcirculararea [areaid ...]\n"
         "\tdelrectanglearea [areaid ...]\n"
         "\tdelpolygonalarea [areaid ...]\n"
         "\tdelroute [routeid ...]\n"
         "\tupgrade device/gps versionid filepath\n"
         "Additional instructions:\n"
         "\tlatitude/longitude -- value in degrees, "
              "accurate to 6 decimal places.\n"
         "\troadsection -- the inflection point to the next inflection point.\n"
         "\thex -- use hexadecimal representation.\n"
         "\tcontrolcommand -- 1: wireless upgrade; 2: connect special server; "
              "3:poweroff; 4: reboot; 5: factoty_reset; "
              "6: trun off data communication; "
              "7: turn off all wireless communication. not support 1 and 2.\n"
         "\tstarttime/endtime -- yymmddhhmmss\n"
         "\tcoordinateitem -- latitude longitude\n"
         "\tcircularareaitem -- id(hex) attribute(hex) coordinateitem radius "
              "[starttime] [endtime] [maxspeed] [overspeedtime]\n"
         "\trectangleareaitem -- id(hex) attribute(hex) coordinateitem1 "
              "coordinateitem2 [starttime] [endtime] "
              "[maxspeed] [overspeedtime]\n"
         "\tpolygonalareaitem -- id(hex) attribute(hex) [starttime] [endtime] "
              "[maxspeed] [overspeedtime] "
              "coordinatecount [coordinateitem ...]\n"
         "\trouteitem -- id(hex) attribute(hex) [starttime] [endtime] "
              "[inflectionpointcount] [inflectionpointitem ...]\n"
         "\tinflectionpointitem -- id(hex) roadsectionid(hex) coordinateitem "
              "roadsectionwide roadsectionattribute(hex) "
              "[maxdrivingtime] [mindrivingtime] [maxspeed] [overspeedtime]\n");
}

int main(int argc, char **argv) {
  std::string command;
  char recv_buf[65536] = {0};

  if (argc < 3) {
    PrintUsage();
    exit(0);
  }

  command.clear();
  for (int i = 1; i < argc; ++i) {
    command += argv[i];
    if (i < (argc - 1))
      command += " ";
  }

  int fd = ClientConnect("/tmp/jt808cmd.sock");
  if (fd > 0) {
    send(fd, command.c_str(), command.length(), 0);
    if (recv(fd, recv_buf, sizeof(recv_buf), 0) > 0)
      printf("%s\n", recv_buf);

    close(fd);
  }

  return 0;
}

