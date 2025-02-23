cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)



add_executable(test1 test1.cpp)
add_executable(correctness correctness.cpp)


target_link_libraries(test1 KVStore)
target_link_libraries(correctness KVStore)




