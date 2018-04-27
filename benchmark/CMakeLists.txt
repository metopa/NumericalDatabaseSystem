cmake_minimum_required(VERSION 3.2)
project(numdb_benchmark)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")


option(BENCHMARK_ENABLE_TESTING "Enable testing of the benchmark library." ON)
option(BENCHMARK_ENABLE_EXCEPTIONS "Enable the use of exceptions in the benchmark library." ON)
option(BENCHMARK_ENABLE_LTO "Enable link time optimisation of the benchmark library." ON)
option(BENCHMARK_USE_LIBCXX "Build and test using libc++ as the standard library." OFF)
option(BENCHMARK_BUILD_32_BITS "Build a 32 bit version of the library" OFF)

add_subdirectory(../3rdparty/google_benchmark google_benchmark)

include_directories(../include)
include_directories(../3rdparty/function_traits/include)
include_directories(../3rdparty/murmurhash2functor/include)
include_directories(../3rdparty/google_benchmark/include)

add_executable(bench ../benchmark/bench.cpp)
target_link_libraries(bench numdb benchmark pthread)
