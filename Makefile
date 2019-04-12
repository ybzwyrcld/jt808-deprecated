CC=
#CC=arm-linux-
CFLAGS=-O2 -Wall -std=c++11

INC=-I./include
LDFLAGS=


all: jt808-service jt808-cmd


jt808-service: main.o jt808_service.o bcd.o unix_socket.o
	$(CC)g++ $^ -pthread -o $@

jt808-cmd: unix_socket.o jt808_cmd.o 	
	$(CC)g++ $^ -o $@

%.o:%.cpp
	$(CC)g++ $(CFLAGS) $(INC) $(LDFLAGS) -o $@ -c $<


install:
	mv jt808-service jt808-cmd run/

clean:
	rm -rf run/* jt808-* *.o

