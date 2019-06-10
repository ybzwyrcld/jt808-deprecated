add_executable(jt808command jt808_command.cc)

target_link_libraries(jt808command PRIVATE
  unix_socket
)
