cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_library(KVStore INTERFACE)

target_include_directories(KVStore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(KVStore 
    INTERFACE MemTable
    INTERFACE SSTable
    INTERFACE SerializeWrapper
)