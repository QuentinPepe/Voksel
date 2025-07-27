cmake_minimum_required(VERSION 3.31.6)
project(Voksel LANGUAGES CXX)

option(VOXEL_BUILD_EXAMPLES "Build examples" ON)
option(VOXEL_USE_DX11 "Use DirectX11" ON)
option(VOXEL_USE_VULKAN "Use Vulkan" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Modules)
#include(Platform)

add_subdirectory(external)
add_subdirectory(engine)
if(VOXEL_BUILD_EXAMPLES)
    add_subdirectory(game)
endif ()