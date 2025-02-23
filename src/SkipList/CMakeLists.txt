cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(SkipList INTERFACE)

target_include_directories(SkipList INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)