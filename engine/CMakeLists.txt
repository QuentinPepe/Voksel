add_subdirectory(core)
add_subdirectory(ecs)
add_subdirectory(graphics)
add_subdirectory(input)
add_subdirectory(tasks)

add_library(voxel_engine INTERFACE)

target_link_libraries(voxel_engine
        INTERFACE
            voxel_core
            voxel_ecs
            voxel_graphics
            voxel_input
            voxel_tasks
)

target_include_directories(voxel_engine
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}
)
