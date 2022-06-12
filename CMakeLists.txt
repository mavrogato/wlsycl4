
cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER icpx)

project(wlsycl4)

find_package(IntelDPCPP REQUIRED)

add_executable(wlsycl4
  main.cc)

target_compile_options(wlsycl4
  PRIVATE
  -Wall
  -std=c++20)

target_link_libraries(wlsycl4
  PRIVATE
  wayland-client)

add_custom_target(run
  DEPENDS wlsycl4
  COMMAND WAYLAND_DEBUG=1 ./wlsycl4)
