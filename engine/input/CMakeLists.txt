add_library(voxel_input STATIC)

add_module_library(voxel_input)

target_link_libraries(voxel_input
        PUBLIC
        voxel_core
        voxel_graphics
        glfw
)