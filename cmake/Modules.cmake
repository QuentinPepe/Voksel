
function(add_module_library target)
    file(GLOB_RECURSE _MODULE_INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.ixx"
    )

    file(GLOB_RECURSE _MODULE_IMPL
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )

    set(_VS_MOD_DIR "$ENV{VCToolsInstallDir}modules")
    set(_STD_IXX "${_VS_MOD_DIR}\\std.ixx")
    if(EXISTS "${_STD_IXX}")
        list(APPEND _MODULE_INTERFACE "${_STD_IXX}")
        set(_MODULE_BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
            "${_VS_MOD_DIR}"
        )
    else ()
        message(WARNING "Impossible de trouver std.ixx dans ${_VS_MOD_DIR}")
        set(_MODULE_BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()

    if(_MODULE_INTERFACE)
        target_sources(${target}
            PUBLIC # TODO Chercher comment ne pas mettre PUBLIC
                FILE_SET CXX_MODULES TYPE CXX_MODULES
                BASE_DIRS ${_MODULE_BASE_DIRS}
                FILES ${_MODULE_INTERFACE}
        )
    endif ()

    if(_MODULE_IMPL)
        target_sources(${target} PRIVATE ${_MODULE_IMPL})
    endif ()

    set_target_options(${target})
endfunction()