
cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "-std=c++1z -Wall")

add_executable(shell main.cc)

target_link_libraries(shell wayland-client wayland-egl GL EGL)

add_custom_target(run
  COMMAND ./shell
  DEPENDS shell
  )
