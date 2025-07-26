set(CMAKE_CXX_STANDARD 23)

function(set_target_options target)
    target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/Zc:__cplusplus;/experimental:module>
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-fmodules-ts>

            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2;/Ob3>
            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3;-march=native>
    )
endfunction()