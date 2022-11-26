cmake_minimum_required(VERSION 3.23)
project(seastar_echo_udp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_CXX_STANDARD 20)

find_package(Seastar REQUIRED)

find_library(FMT_LIB NAMES fmt HINTS ENV FMT_V8_LIB_PATH REQUIRED)
if(FMT_LIB)
    message(STATUS "Found fmt library v8 - ${FMT_LIB}")
endif()

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server Seastar::seastar ${FMT_LIB})

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client Seastar::seastar ${FMT_LIB})
