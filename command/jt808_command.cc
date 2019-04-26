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
  cout << "Usage: jt808-cmd [options]" << endl;
  cout << "Options:" << endl;
  cout << "update [phone number] [system/device/gpsfw/cdrfw] [version id] [file path]" << endl;

  exit(0);
}

int main(int argc, char **argv) {
  string command;

  if (argc != 6) {
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

  cout << command << endl;

  int fd = ClientConnect("/tmp/jt808cmd.sock");
  if (fd > 0) {
    send(fd, command.c_str(), command.length(), 0);
    if (command.find("get") != string::npos) {
      char recv_buf[1024] = {0};
      if(recv(fd, recv_buf, sizeof(recv_buf), 0) > 0)
        printf("%s\n", recv_buf);
    }

    close(fd);
  }

  return 0;
}

