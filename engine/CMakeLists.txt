add_library(voxel_engine STATIC)
add_module_library(voxel_engine)
target_link_libraries(voxel_engine
        PUBLIC
        glfw
)
target_link_libraries(voxel_engine PUBLIC directx12 stb_headers)