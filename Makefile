CC=
#CC=arm-linux-
CFLAGS=-O2 -Wall -std=c++11

INC=-I.
LDFLAGS=


all: jt808-service jt808-command


jt808-service: main.o \
	bcd/bcd.o \
	service/jt808_service.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -pthread -o $@

jt808-command: command/jt808_command.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -o $@

%.o:%.cc
	$(CC)g++ $(CFLAGS) $(INC) $(LDFLAGS) -o $@ -c $<


install:
	mv jt808-service jt808-command run/

clean:
	rm -rf run/* jt808-* *.o service/*.o command/*.o bcd/*.o unix_socket/*.o

