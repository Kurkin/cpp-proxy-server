cmake_minimum_required(VERSION 2.8)
project(proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
        "proxy/new_http_handler.cpp"
        "proxy/new_http_handler.hpp"
        "proxy/kqueue.cpp"
        "proxy/kqueue.hpp"
        "proxy/main.cpp"
        "proxy/proxy.cpp"
        "proxy/proxy.hpp"
        "proxy/throw_error.cpp"
        "proxy/throw_error.h"
        "proxy/socket.cpp"
        "proxy/socket.hpp"
        "proxy/utils.hpp"
        "proxy/file_descriptor.cpp"
        "proxy/file_descriptor.h"
        "proxy/timer.cpp"
        "proxy/timer.h"
)

add_executable(proxy_serv ${SOURCE_FILES})

target_link_libraries(proxy_serv pthread)