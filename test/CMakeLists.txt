cmake_minimum_required(VERSION 3.2)
project(numdb_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

enable_testing()
add_subdirectory(../3rdparty/gtest gtest)

include_directories(../include)
include_directories(../3rdparty/gtest/googletest/include)
include_directories(../3rdparty/function_traits/include)
include_directories(../3rdparty/murmurhash2functor/include)
include_directories(../3rdparty/murmurhash2functor/include)

add_executable(unit_tests
        fair_lru_test.cpp
        fixed_hashtable_fair_lru_test.cpp
        splay_tree_test.cpp)
target_link_libraries(unit_tests numdb gtest gtest_main pthread)

add_test(UnitTests unit_tests)
