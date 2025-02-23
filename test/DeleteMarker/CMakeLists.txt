cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)


set(SOURCE_FILES DeleteMarker.cpp)

add_executable(test_DeleteMarker ${SOURCE_FILES})

target_link_libraries(test_DeleteMarker DeleteMarker)