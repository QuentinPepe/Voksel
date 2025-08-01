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

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:game>/shaders"
)