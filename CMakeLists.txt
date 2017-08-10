cmake_minimum_required(VERSION 3.4)
project(LinuxIPC)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")

set(SOURCE_FILES counter.c)

add_executable(LinuxIPC ${SOURCE_FILES})