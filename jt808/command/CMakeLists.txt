cmake_minimum_required (VERSION 2.6)

project (jt808command)

set (JT808_COMMAND_VERSION_MAJOR 1)
set (JT808_COMMAND_VERSION_MINOR 0)

set (CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set (CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wfatal-errors")

include_directories (../)

set (SOURCEFILES
  jt808_command.cc
  ../unix_socket/unix_socket.cc
  )

add_executable (jt808command ${SOURCEFILES})
