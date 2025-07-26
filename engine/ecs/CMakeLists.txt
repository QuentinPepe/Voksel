add_library(voxel_ecs STATIC)

add_module_library(voxel_ecs)

target_link_libraries(voxel_ecs
        PUBLIC
            voxel_core
)