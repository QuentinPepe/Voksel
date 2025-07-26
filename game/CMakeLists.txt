add_executable(game
    main.cpp
)

set(_VS_MOD_DIR "$ENV{VCToolsInstallDir}modules")
set(_STD_IXX "${_VS_MOD_DIR}\\std.ixx")

target_link_libraries(game
    PRIVATE
        voxel_engine
)

set_target_options(game)