cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(Hash INTERFACE)

target_include_directories(Hash INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)