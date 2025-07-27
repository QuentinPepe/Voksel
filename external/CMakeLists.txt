include(FetchContent)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# DirectX 11 libs (Windows only)
if(WIN32 AND VOXEL_USE_DX11)
    find_path(D3D11_INCLUDE_DIR d3d11.h
            HINTS
            "$ENV{DXSDK_DIR}/Include"
            "$ENV{ProgramFiles}/Windows Kits/10/Include/*/um"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Include/um"
    )

    find_library(D3D11_LIBRARY d3d11.lib
            HINTS
            "$ENV{DXSDK_DIR}/Lib/x64"
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(D3DCOMPILER_LIBRARY d3dcompiler.lib
            HINTS
            "$ENV{DXSDK_DIR}/Lib/x64"
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(DXGI_LIBRARY dxgi.lib
            HINTS
            "$ENV{DXSDK_DIR}/Lib/x64"
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    if(NOT D3D11_INCLUDE_DIR OR NOT D3D11_LIBRARY)
        message(FATAL_ERROR "DirectX 11 not found")
    endif()

    add_library(directx11 INTERFACE)
    target_include_directories(directx11 INTERFACE ${D3D11_INCLUDE_DIR})
    target_link_libraries(directx11 INTERFACE
            ${D3D11_LIBRARY}
            ${D3DCOMPILER_LIBRARY}
            ${DXGI_LIBRARY}
    )
endif()

# Vulkan
if(VOXEL_USE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()