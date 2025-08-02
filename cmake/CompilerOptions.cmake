set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_MODULE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")

if(MSVC)
    add_compile_options(/wd4201 /wd4324)
endif()

function(set_target_options target)
    target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/Zc:__cplusplus;/experimental:module>
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-fmodules-ts>

            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2;/Ob2>
            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3;-march=native>
    )
endfunction()