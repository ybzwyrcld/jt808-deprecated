cmake_minimum_required (VERSION 2.6)

project (jt808service)

set (JT808_SERVICE_VERSION_MAJOR 1)
set (JT808_SERVICE_VERSION_MINOR 0)

set (CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set (CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wfatal-errors")

include_directories (.)

set (SOURCEFILES
  main.cc
  bcd/bcd.cc
  service/jt808_service.cc
  service/jt808_terminal_parameters.cc
  service/jt808_position_report.cc
  service/jt808_util.cc
  unix_socket/unix_socket.cc
  )

add_executable (jt808service ${SOURCEFILES})
