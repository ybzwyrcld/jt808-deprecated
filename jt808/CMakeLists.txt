cmake_minimum_required(VERSION 3.5)

project(jt808service)

set(JT808_SERVICE_VERSION_MAJOR 1)
set(JT808_SERVICE_VERSION_MINOR 0)

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wfatal-errors")

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")

configure_file(
  ${PROJECT_SOURCE_DIR}/devicelist/devices.txt
  ${PROJECT_BINARY_DIR}/devicelist/devices.txt
  COPYONLY
)

add_subdirectory(googletest)
add_subdirectory(bcd)
add_subdirectory(unix_socket)

add_subdirectory(common)
add_subdirectory(terminal)
add_subdirectory(service)

add_executable(jt808service main/service_main.cc)

target_link_libraries(jt808service PRIVATE
  jt808_service
)

add_executable(jt808terminal main/terminal_main.cc)

target_link_libraries(jt808terminal PRIVATE
  jt808_terminal
)

add_executable(jt808command main/command_main.cc)

target_link_libraries(jt808command PRIVATE
  unix_socket
)
