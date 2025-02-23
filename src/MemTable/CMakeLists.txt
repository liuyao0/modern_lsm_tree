cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(MemTable INTERFACE)

target_include_directories(MemTable INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MemTable 
    INTERFACE SkipList 
    INTERFACE DeleteMarker)