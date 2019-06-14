CC=
#CC=arm-linux-
CFLAGS=-O2 -Wall -std=c++11

INC=-I.
LDFLAGS=


all: jt808service jt808terminal jt808command


jt808service: main/service_main.o \
	bcd/bcd.o \
	common/jt808_terminal_parameters.o \
	common/jt808_util.o \
	service/jt808_service.o \
	service/jt808_position_report.o \
	service/jt808_util.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -pthread -o $@

jt808terminal: main/terminal_main.o \
	bcd/bcd.o \
	common/jt808_terminal_parameters.o \
	common/jt808_util.o \
	terminal/jt808_terminal.o \
	terminal/jt808_terminal_parameters.o \
	terminal/jt808_area_route.o
	$(CC)g++ $^ -o $@

jt808command: main/command_main.o \
	unix_socket/unix_socket.o
	$(CC)g++ $^ -o $@

%.o:%.cc
	$(CC)g++ $(CFLAGS) $(INC) $(LDFLAGS) -o $@ -c $<


install:
	$(CC)strip jt808service jt808terminal jt808command


clean:
	rm -rf jt808service jt808terminal jt808command
	rm -rf bcd/*.o unix_socket/*.o common/*.o service/*.o terminal/*.o main/*.o

