#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unix_socket.h>

#include <iostream>
#include <fstream>
#include <string>


using std::cout;
using std::endl;
using std::string;

void print_usage(void)
{
	cout << "Usage: skconfig [options]" << endl;
	cout << "Options:" << endl;
	cout << "update [phone num] [device/gpsfw] [version id] [path]" << endl;

	exit(0);
}

int main(int argc, char **argv)
{
	string s, s1;

	if (argc != 6) {
		print_usage();
	}

	s = argv[1];
	s += " ";
	s += argv[2];

	if (argc >= 4) {
		s += " ";
		s += argv[3];
	}

	if (argc >= 5) {
		s += " ";
		s += argv[4];
	}

	if (argc >= 6) {
		s += " ";
		s += argv[5];
	}

	cout << s << endl;

	int fd = cli_conn("/tmp/jt808cmd.sock");
	if (fd > 0) {
		send(fd, s.c_str(), s.length(), 0);
		if (s.find("get") != string::npos) {
			char recv_buf[1024] = {0};
			if(recv(fd, recv_buf, sizeof(recv_buf), 0) > 0)
				printf("%s\n", recv_buf);
		}

		close(fd);
	}

	return 0;
}

