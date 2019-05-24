#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include "unix_socket/unix_socket.h"


static inline void PrintUsage(void) {
  printf("Usage: jt808-command phonenum [options ...]\n"
         "Options:\n"
         "get startup/gps/cdradio/ntripcors/ntripservice/jt808service\n"
         "set startup [gps] [cdradio] [ntripcors] [ntripservice] "
                     "[jt808service]\n"
         "    gps [LOGGGA] [LOGRMC] [LOGATT]\n"
         "    cdradio bauderate workfreqpoint recvmode formcode\n"
         "    ntripcors ip port user passwd mntpoint reportinterval\n"
         "    ntripservice ip port user passwd mntpont reportinterval\n"
         "    jt808service ip port phonenum reportinterval\n"
         "getterminalparameter [parameterid ...]\n"
         "setterminalparameter [parameterid(HEX):parametervalue ...]\n"
         "setcirculararea update/append/modify [areaid(hex) areaattr(hex) "
                         "latitude longitude radius "
                         "[starttime(yy-mm-dd-hh-mm-ss)] [endtime] "
                         "[maxspeed] [overspeedtime] ...]\n"
         "setrectanglearea update/append/modify [areaid(hex) areaattr(hex) "
                          "coordinate1(latitude longitude) coordinate2 "
                          "[starttime(yy-mm-dd-hh-mm-ss)] [endtime] "
                          "[maxspeed] [overspeedtime] ...]\n"
         "setpolygonalarea update/append/modify [areaid(hex) areaattr(hex) "
                          "[starttime(yy-mm-dd-hh-mm-ss)] [endtime] "
                          "[maxspeed] [overspeedtime] coordinatecount "
                          "[[latitude longitude] ...] ...]\n"
         "delcirculararea [areaid ...]\n"
         "delrectanglearea [areaid ...]\n"
         "delpolygonalarea [areaid ...]\n"
         "upgrade system/device/gps/cdradio versionid filepath\n");

  exit(0);
}

int main(int argc, char **argv) {
  std::string command;
  char recv_buf[65536] = {0};

  if (argc < 3) {
    PrintUsage();
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

