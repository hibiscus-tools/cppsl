cmake_minimum_required(VERSION 3.20)

project(cppsl CXX)

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_library(cppsl
	source/ceval.cpp
	source/compress.cpp
	source/gir.cpp
	source/translate.cpp)

add_executable(cube examples/cube.cpp)
add_executable(features examples/features.cpp)
add_executable(triangle examples/triangle.cpp)
add_executable(mesh examples/mesh.cpp)

target_compile_options(cube PUBLIC -Wall)
target_compile_options(features PUBLIC -Wall)
target_compile_options(triangle PUBLIC -Wall)
target_compile_options(mesh PUBLIC -Wall)

include_directories(. include examples examples/littlevk/examples/glm)

set(LIBRARIES -fsanitize=address cppsl fmt assimp
	glfw SPIRV glslang::glslang Vulkan::Vulkan
	glslang::glslang-default-resource-limits)

target_link_libraries(cube ${LIBRARIES})
target_link_libraries(features ${LIBRARIES})
target_link_libraries(triangle ${LIBRARIES})
target_link_libraries(mesh ${LIBRARIES})
