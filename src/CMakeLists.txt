cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

add_subdirectory(DeleteMarker)
add_subdirectory(SerializeWrapper)
add_subdirectory(SkipList)
add_subdirectory(MemTable)
add_subdirectory(Hash)
add_subdirectory(SSTable)
add_subdirectory(KVStore)
