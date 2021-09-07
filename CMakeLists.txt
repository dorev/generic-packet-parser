cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(tests)

add_executable(tests
    tests.cpp
    genericpacketparser.h
)
