cmake_minimum_required(VERSION 3.22)

project(perfcxx LANGUAGES CXX)

add_library(perfcxx perf-lib.cpp)
target_include_directories(perfcxx PUBLIC ${CMAKE_CURRENT_LIST_DIR})