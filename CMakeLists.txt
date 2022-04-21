cmake_minimum_required(VERSION 3.22)
project(IPK2)

set(CMAKE_CXX_STANDARD 11)

add_executable(IPK2Server server.cpp debug-headers.h serve_client.cpp serve_client.h)
add_executable(IPK2Client client.cpp debug-headers.h)