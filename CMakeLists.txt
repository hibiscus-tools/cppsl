cmake_minimum_required(VERSION 3.20)

project(cppsl CXX)

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_library(cppsl
	source/ceval.cpp
	source/gir.cpp
	source/translate.cpp)

add_executable(features examples/features.cpp)
add_executable(triangle examples/triangle.cpp)

target_compile_options(features PUBLIC -Wall)
target_compile_options(triangle PUBLIC -Wall)

include_directories(. include examples)

target_link_libraries(features -fsanitize=address cppsl fmt
	glfw SPIRV glslang::glslang Vulkan::Vulkan
	glslang::glslang-default-resource-limits)

target_link_libraries(triangle -fsanitize=address cppsl fmt
	glfw SPIRV glslang::glslang Vulkan::Vulkan
	glslang::glslang-default-resource-limits)
