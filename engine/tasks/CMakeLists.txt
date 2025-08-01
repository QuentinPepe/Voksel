add_library(voxel_tasks STATIC)

add_module_library(voxel_tasks)

target_link_libraries(voxel_tasks
        PUBLIC
        voxel_core
        voxel_graphics
        voxel_ecs
        voxel_input
)