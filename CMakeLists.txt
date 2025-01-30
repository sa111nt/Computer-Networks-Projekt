cmake_minimum_required(VERSION 3.10)

project(SK_project)

set(CMAKE_CXX_STANDARD 20)

set(COMPILE_FLAGS "-Wall -O0 -g -pthread")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_FLAGS}")

add_executable(SK_project server.cpp)
