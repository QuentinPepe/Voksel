# Create a shared std module target
function(create_std_module_target)
    if (NOT TARGET std_module)
        set(_VS_MOD_DIR "$ENV{VCToolsInstallDir}modules")
        set(_STD_IXX "${_VS_MOD_DIR}\\std.ixx")

        if (EXISTS "${_STD_IXX}")
            add_library(std_module STATIC)
            target_sources(std_module
                    PUBLIC
                    FILE_SET CXX_MODULES TYPE CXX_MODULES
                    BASE_DIRS "${_VS_MOD_DIR}"
                    FILES "${_STD_IXX}"
            )
            set_target_options(std_module)
            message(STATUS "Module std créé depuis ${_STD_IXX}")
        else ()
            message(FATAL_ERROR "Impossible de trouver std.ixx dans ${_VS_MOD_DIR}")
        endif ()
    endif ()
endfunction()

function(add_module_library target)
    file(GLOB_RECURSE _MODULE_INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/*.ixx"
    )

    file(GLOB_RECURSE _MODULE_IMPL
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )

    if (_MODULE_INTERFACE)
        target_sources(${target}
                PUBLIC
                FILE_SET CXX_MODULES TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                FILES ${_MODULE_INTERFACE}
        )
    endif ()

    if (_MODULE_IMPL)
        target_sources(${target} PRIVATE ${_MODULE_IMPL})
    endif ()

    # Link to std module
    create_std_module_target()
    target_link_libraries(${target} PUBLIC std_module)

    set_target_options(${target})
endfunction()