cmake_minimum_required(VERSION 3.31.6)
project(Voksel LANGUAGES CXX)

option(VOXEL_BUILD_EXAMPLES "Build examples" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Modules)

add_subdirectory(external)
add_subdirectory(engine)
if(VOXEL_BUILD_EXAMPLES)
    add_subdirectory(game)
endif()