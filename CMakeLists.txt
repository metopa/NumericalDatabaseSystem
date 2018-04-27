cmake_minimum_required(VERSION 3.2)
project(numdb)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(3rdparty/murmurhash2functor/lib)
add_subdirectory(lib)
add_subdirectory(test)
add_subdirectory(benchmark)




