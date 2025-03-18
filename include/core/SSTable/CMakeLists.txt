cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(SSTable INTERFACE)

target_include_directories(SSTable INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SSTable INTERFACE Hash)