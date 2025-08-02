add_library(voxel_maths STATIC)

add_module_library(voxel_maths)

target_link_libraries(voxel_maths
        PUBLIC
        voxel_core
)