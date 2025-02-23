cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)


set(SOURCE_FILES SkipList.cpp)

add_executable(test_SkipList ${SOURCE_FILES})

target_link_libraries(test_SkipList SkipList)