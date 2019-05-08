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

using std::cout;
using std::endl;
using std::string;

static inline void PrintUsage(void) {
  printf("Usage: jt808-command phonenum [options]\n");
  printf("Options:\n");
  printf("get startup/gps/cdradio/ntripcors/ntripservice/jt808service\n");
  printf("set startup [gps] [cdradio] [ntripcors] [ntripservice] "
                      "[jt808service]\n");
  printf("    gps [LOGGGA] [LOGRMC] [LOGATT]\n");
  printf("    cdradio bauderate workfreqpoint recvmode formcode\n");
  printf("    ntripcors ip port user passwd mntpoint reportinterval\n");
  printf("    ntripservice ip port user passwd mntpont reportinterval\n");
  printf("    jt808service ip port phonenum reportinterval\n");
  printf("upgrade system/device/gps/cdradio versionid filepath\n");

  exit(0);
}

int main(int argc, char **argv) {
  string command;
  char recv_buf[1024] = {0};

  if (argc < 4 || argc > 11) {
    PrintUsage();
  }

  command.clear();
  for (int i = 1; i < argc; ++i) {
    command += argv[i];
    if (i < (argc - 1))
      command += " ";
  }

  // printf("command: %s\n", command.c_str());

  int fd = ClientConnect("/tmp/jt808cmd.sock");
  if (fd > 0) {
    send(fd, command.c_str(), command.length(), 0);
    if (recv(fd, recv_buf, sizeof(recv_buf), 0) > 0)
      printf("%s\n", recv_buf);

    close(fd);
  }

  return 0;
}

