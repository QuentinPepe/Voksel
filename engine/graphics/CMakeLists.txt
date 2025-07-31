add_library(voxel_graphics STATIC)

add_module_library(voxel_graphics)

target_link_libraries(voxel_graphics
        PUBLIC
        voxel_core
        glfw
)

target_link_libraries(voxel_graphics PUBLIC directx12)
