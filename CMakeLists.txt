# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.1)

list(APPEND CMAKE_PREFIX_PATH include)
set(CMAKE_CXX_STANDARD 17)

project (ResourceCompiler)

add_executable(compilerc
	src/ResourceCompiler.cpp
)