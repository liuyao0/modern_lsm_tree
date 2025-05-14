cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)



add_executable(test1 test1.cpp)

target_link_libraries(test1 KVStore Threads::Threads)




