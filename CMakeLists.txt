cmake_minimum_required(VERSION 3.16)
project(IPK2)

set(CMAKE_CXX_STANDARD 11)

add_executable(ipk-simpleftp-server server.cpp debug-headers.h serve_client.cpp serve_client.h)
add_executable(ipk-simpleftp-client client.cpp debug-headers.h)
