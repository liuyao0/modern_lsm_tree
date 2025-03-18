cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_subdirectory(DeleteMarker)
add_subdirectory(Hash)

add_library(SerializeWrapper INTERFACE)

target_include_directories(SerializeWrapper INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

