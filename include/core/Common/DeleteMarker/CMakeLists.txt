cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(DeleteMarker INTERFACE)
target_include_directories(DeleteMarker INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)