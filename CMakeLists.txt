cmake_minimum_required(VERSION 3.3)
project(Proxy_server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    "Proxy server/http_handler.cpp"
    "Proxy server/http_handler.hpp"
    "Proxy server/kqueue.cpp"
    "Proxy server/kqueue.hpp"
    "Proxy server/main.cpp"
    "Proxy server/proxy.cpp"
    "Proxy server/proxy.hpp"
    "Proxy server/throw_error.cpp"
    "Proxy server/throw_error.h"
    "Proxy server/utils.cpp"
    "Proxy server/utils.hpp")

add_executable(Proxy_server ${SOURCE_FILES})