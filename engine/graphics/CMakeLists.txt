add_library(voxel_graphics STATIC)

add_module_library(voxel_graphics)

if(VOXEL_USE_DX11)
    target_compile_definitions(voxel_graphics PUBLIC VOXEL_USE_DX11)
elseif(VOXEL_USE_VULKAN)
    target_compile_definitions(voxel_graphics PUBLIC VOXEL_USE_VULKAN)
endif()

target_link_libraries(voxel_graphics
        PUBLIC
        voxel_core
        glfw
)

if(WIN32 AND VOXEL_USE_DX11)
    target_link_libraries(voxel_graphics PUBLIC directx11)
endif()

if(VOXEL_USE_VULKAN)
    target_link_libraries(voxel_graphics PUBLIC Vulkan::Vulkan)
endif()