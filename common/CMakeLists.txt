add_library(common_jt808_util STATIC
  jt808_util.cc
)

add_library(common_terminal_parameter STATIC
  jt808_terminal_parameters.cc
)

target_link_libraries(common_terminal_parameter PRIVATE
  bcd
)

#add_executable(jt808_test
#  jt808_test.cc
#)

#target_link_libraries(jt808_test PRIVATE
#  jt808
#  gtest
#  gtest_main
#)
