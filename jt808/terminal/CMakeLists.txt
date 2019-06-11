add_library(jt808_area_route STATIC
  jt808_area_route.cc
)

add_library(terminal_terminal_parameter STATIC
  jt808_terminal_parameters.cc
)

target_link_libraries(terminal_terminal_parameter PRIVATE
  bcd
)

add_library(jt808_terminal STATIC
  jt808_terminal.cc
)

target_link_libraries(jt808_terminal PRIVATE
  bcd
  jt808_area_route
  common_jt808_util
  common_terminal_parameter
  terminal_terminal_parameter
)

add_executable(jt808_terminal_test
  jt808_terminal_test.cc
)

target_link_libraries(jt808_terminal_test PRIVATE
  jt808_terminal
  gmock_main
)
