add_executable(game
        main.cpp
)

create_std_module_target()
target_link_libraries(game
        PRIVATE
        std_module
        voxel_engine
)

set_target_options(game)