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
  printf("Usage: jt808-command [phone num] [options]\n");
  printf("Options:");
  printf("get [startup/gps/cdradio/ntripcors/ntripservice/jt808service]");
  printf("upgrade [system/device/gps/cdradio] [version id] [file path]");

  exit(0);
}

int main(int argc, char **argv) {
  string command;

  if (argc < 3 || argc > 7) {
    PrintUsage();
  }

  command = argv[1];
  command += " ";
  command += argv[2];

  if (argc >= 4) {
    command += " ";
    command += argv[3];
  }

  if (argc >= 5) {
    command += " ";
    command += argv[4];
  }

  if (argc >= 6) {
    command += " ";
    command += argv[5];
  }

  printf("command: %s\n", command.c_str());

  int fd = ClientConnect("/tmp/jt808cmd.sock");
  if (fd > 0) {
    send(fd, command.c_str(), command.length(), 0);
    if (command.find("get") != string::npos) {
      char recv_buf[1024] = {0};
      if(recv(fd, recv_buf, sizeof(recv_buf), 0) > 0)
        printf("result: %s\n", recv_buf);
    }

    close(fd);
  }

  return 0;
}

