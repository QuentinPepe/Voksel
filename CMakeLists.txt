cmake_minimum_required(VERSION 3.31.6)
project(Voksel LANGUAGES CXX)

option(VOXEL_BUILD_EXAMPLES "Build examples" ON)
option(VOXEL_USE_DX11 "Use DirectX11" ON)
option(VOXEL_USE_VULKAN "Use Vulkan" OFF)

if(VOXEL_USE_DX11 AND VOXEL_USE_VULKAN)
    message(FATAL_ERROR "Cannot enable both DirectX11 and Vulkan at the same time")
endif()

if(NOT VOXEL_USE_DX11 AND NOT VOXEL_USE_VULKAN)
    message(FATAL_ERROR "Must enable at least one render backend")
endif()

if(VOXEL_USE_DX11 AND NOT WIN32)
    message(FATAL_ERROR "DirectX11 is only available on Windows")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Modules)

add_subdirectory(external)
add_subdirectory(engine)
if(VOXEL_BUILD_EXAMPLES)
    add_subdirectory(game)
endif()