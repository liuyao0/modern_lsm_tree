cmake_minimum_required(VERSION 3.10)

project(lsm_kvstore)

option(EXAMPLE_LINK_SO "Whether examples are linked dynamically" OFF)
option(LINK_TCMALLOC "Link tcmalloc if possible" ON)

# execute_process(
#     COMMAND bash -c "find ${CMAKE_SOURCE_DIR}/../.. -type d -path \"*output/include/braft\" | xargs dirname | xargs dirname | tr -d '\n'"
#     OUTPUT_VARIABLE OUTPUT_PATH
# )


list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/braft")
message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

include(FindThreads)
include(FindProtobuf)

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER kvstore.proto)
# include PROTO_HEADER
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)

if(EXAMPLE_LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
    find_library(BRAFT_LIB NAMES braft)
else()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
    find_library(BRAFT_LIB NAMES libbraft.a braft)
endif()

if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif()
include_directories(${BRPC_INCLUDE_PATH})

find_path(BRAFT_INCLUDE_PATH NAMES braft/raft.h)
if ((NOT BRAFT_INCLUDE_PATH) OR (NOT BRAFT_LIB))
    message (FATAL_ERROR "Fail to find braft")
endif()
include_directories(${BRAFT_INCLUDE_PATH})

find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif()
include_directories(${GFLAGS_INCLUDE_PATH})

execute_process(
        COMMAND bash -c "grep \"namespace [_A-Za-z0-9]\\+ {\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $2}' | tr -d '\n'"
        OUTPUT_VARIABLE GFLAGS_NS
)
if(${GFLAGS_NS} STREQUAL "GFLAGS_NAMESPACE")
    execute_process(
            COMMAND bash -c "grep \"#define GFLAGS_NAMESPACE [_A-Za-z0-9]\\+\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $3}' | tr -d '\n'"
            OUTPUT_VARIABLE GFLAGS_NS
    )
endif()

if (LINK_TCMALLOC)
    find_path(GPERFTOOLS_INCLUDE_DIR NAMES gperftools/heap-profiler.h)
    find_library(GPERFTOOLS_LIBRARIES NAMES tcmalloc_and_profiler)
    if (GPERFTOOLS_INCLUDE_DIR AND GPERFTOOLS_LIBRARIES)
        set(CMAKE_CXX_FLAGS "-DBRPC_ENABLE_CPU_PROFILER")
        include_directories(${GPERFTOOLS_INCLUDE_DIR})
    else ()
        set (GPERFTOOLS_LIBRARIES "")
    endif ()
endif ()

set(CMAKE_CPP_FLAGS "-DGFLAGS_NS=${GFLAGS_NS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CPP_FLAGS} -DNDEBUG -O2 -D__const__=__unused__ -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # require at least gcc 4.8
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(FATAL_ERROR "GCC is too old, please install a newer version supporting C++11")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # require at least clang 3.3
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.3)
        message(FATAL_ERROR "Clang is too old, please install a newer version supporting C++11")
    endif()
else()
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif()

if(CMAKE_VERSION VERSION_LESS "3.1.3")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
    endif()
else()
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})

add_executable(kvstore_client client.cpp ${PROTO_SRC} ${PROTO_HEADER})
add_executable(kvstore_server server.cpp ${PROTO_SRC} ${PROTO_HEADER})

set(DYNAMIC_LIB
        ${CMAKE_THREAD_LIBS_INIT}
        ${GFLAGS_LIBRARY}
        ${PROTOBUF_LIBRARY}
        ${GPERFTOOLS_LIBRARIES}
        ${LEVELDB_LIB}
        ${BRAFT_LIB}
        ${BRPC_LIB}
        rt
        ssl
        crypto
        dl
        z
        KVStore
)

target_link_libraries(kvstore_client
        "-Xlinker \"-(\""
        ${DYNAMIC_LIB}
        "-Xlinker \"-)\"")

target_link_libraries(kvstore_server
        "-Xlinker \"-(\""
        ${DYNAMIC_LIB}
        "-Xlinker \"-)\"")

add_executable(correctness correctness.cpp)

target_link_libraries(correctness KVStore Threads::Threads)
