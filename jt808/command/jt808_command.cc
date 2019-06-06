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
         "\tsetcirculararea update/append/modify [circularareaitem ...]\n"
         "\tsetrectanglearea update/append/modify [rectangleareaitem ...]\n"
         "\tsetpolygonalarea update/append/modify [polygonalareaitem ...]\n"
         "\tsetroute update/append/modify [routeitem ...] \n"
         "\tdelcirculararea [areaid ...]\n"
         "\tdelrectanglearea [areaid ...]\n"
         "\tdelpolygonalarea [areaid ...]\n"
         "\tdelroute [routeid ...]\n"
         "\tupgrade device versionid filepath\n"
         "\tupgrade gps versionid filepath\n"
         "Additional instructions:\n"
         "\tlatitude/longitude -- value in degrees, "
              "accurate to 6 decimal places.\n"
         "\troadsection -- the inflection point to the next inflection point.\n"
         "\thex -- use hexadecimal representation.\n"
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
              "[maxdrivingtime] [mindrivingtime] [maxspeed] [overspeedtime]\n"
         "Customization options:\n"
         "\tget startup/gps/cdradio/ntripcors/ntripservice/jt808service\n"
         "\tset startup [gps] [cdradio] [ntripcors] [ntripservice] "
              "[jt808service]\n"
         "\tset gps [LOGGGA] [LOGRMC] [LOGATT]\n"
         "\tset cdradio bauderate workfreqpoint recvmode formcode\n"
         "\tset ntripcors ip port user passwd mntpoint reportinterval\n"
         "\tset ntripservice ip port user passwd mntpont reportinterval\n"
         "\tset jt808service ip port phonenum reportinterval\n"
         "\tupgrade system versionid filepath\n"
         "\tupgrade cdradio versionid filepath\n");
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

