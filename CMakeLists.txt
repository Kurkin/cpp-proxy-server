cmake_minimum_required(VERSION 3.3)
project(proxy_server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        "proxy/http_handler.cpp"
        "proxy/http_handler.hpp"
        "proxy/kqueue.cpp"
        "proxy/kqueue.hpp"
        "proxy/main.cpp"
        "proxy/proxy.cpp"
        "proxy/proxy.hpp"
        "proxy/throw_error.cpp"
        "proxy/throw_error.h"
        "proxy/utils.cpp"
        "proxy/utils.hpp")

add_executable(proxy_server ${SOURCE_FILES})

target_link_libraries(proxy_server pthread)