add_executable(math_tests
        core_tests.cpp
        vector_tests.cpp
        matrix_tests.cpp
)

target_link_libraries(math_tests
        PRIVATE
        voxel_engine
        Catch2::Catch2WithMain
)

add_test(NAME Math.UnitTests COMMAND math_tests)
