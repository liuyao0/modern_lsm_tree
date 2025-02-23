cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)



add_executable(test_BloomFilter BloomFilter.cpp)

target_link_libraries(test_BloomFilter SSTable)