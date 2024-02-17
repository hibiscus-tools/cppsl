cmake_minimum_required(VERSION 3.20)

project(cppsl CXX)

include(FetchContent)

FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG master)
FetchContent_MakeAvailable(fmt)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(main source/main.cpp source/format.cpp source/stream.cpp)

target_compile_options(main PRIVATE -Wall -fconcepts-diagnostics-depth=4)
target_link_libraries(main fmt::fmt)