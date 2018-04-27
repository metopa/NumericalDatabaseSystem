cmake_minimum_required(VERSION 3.2)
project(numdb_lib)

set(CMAKE_CXX_STANDARD 14)

include_directories(../include)
include_directories(../3rdparty/function_traits/include)
include_directories(../3rdparty/murmurhash2functor/include)

add_library(numdb fair_lru.cpp fair_lfu.cpp)
target_link_libraries(numdb murmurhash2)


