cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)


add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(test)