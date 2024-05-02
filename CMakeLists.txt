cmake_minimum_required(VERSION 3.20)

project(cppsl CXX)

add_library(cppsl source/ceval.cpp source/translate.cpp)

add_executable(main main.cpp)

include_directories(. include)

target_link_libraries(main cppsl fmt)
