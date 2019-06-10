add_library(jt808_position_report STATIC
  jt808_position_report.cc
)

add_library(jt808_util STATIC
  jt808_util.cc
)

add_library(terminal_parameter STATIC
  jt808_terminal_parameters.cc
)

target_link_libraries(terminal_parameter PRIVATE
  bcd
)

add_library(jt808_service STATIC
  jt808_service.cc
)

target_link_libraries(jt808_service PRIVATE
  bcd
  jt808_position_report
  terminal_parameter
  unix_socket
  jt808_util
)

#add_executable(jt808_test
#  jt808_test.cc
#)

#target_link_libraries(jt808_test PRIVATE
#  jt808
#  gtest
#  gtest_main
#)
