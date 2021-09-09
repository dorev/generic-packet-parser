cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(tests)

add_executable(tests
    tests.cpp
    genericpacketparser.h
)

# GoogleTest
target_include_directories(tests PRIVATE "gtest/googletest/include")
target_link_directories(tests PRIVATE "gtest/lib/Debug" "gtest/lib/Release")
target_link_libraries(tests gtest_main$<$<CONFIG:Debug>:d> gtest$<$<CONFIG:Debug>:d>)
