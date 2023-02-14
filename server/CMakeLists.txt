cmake_minimum_required(VERSION 3.0)
project(ipc_test_server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

## SOURCES  ####################################################################
set(SOURCE_FILES

ipc_server.cpp

task.h
task.cpp

SynchronousSocket.cpp
SynchronousSocket.h
)
##################################################################################

link_libraries(stdc++)

add_executable(ipc_test_server ${SOURCE_FILES} ./ipc_server.cpp)

