CC=
#CC=arm-linux-
CFLAGS=-O2 -Wall -std=c++11

INC=-I.
LDFLAGS=


all: jt808service jt808command


jt808service: main.o \
	bcd/bcd.o \
	service/jt808_service.o \
	service/jt808_terminal_parameters.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -pthread -o $@

jt808command: command/jt808_command.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -o $@

%.o:%.cc
	$(CC)g++ $(CFLAGS) $(INC) $(LDFLAGS) -o $@ -c $<


install:
	$(CC)strip jt808service jt808command


clean:
	rm -rf jt808service jt808command
	rm -rf *.o service/*.o command/*.o bcd/*.o unix_socket/*.o

