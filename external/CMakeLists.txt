include(FetchContent)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# D3DX12 headers
FetchContent_Declare(
        d3dx12
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG        v1.614.0
)
FetchContent_MakeAvailable(d3dx12)

# DirectX 12 libs (Windows only)
if(WIN32)
    find_path(D3D12_INCLUDE_DIR d3d12.h
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/Include/*/um"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Include/um"
    )

    find_library(D3D12_LIBRARY d3d12.lib
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(DXGI_LIBRARY dxgi.lib
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(D3DCOMPILER_LIBRARY d3dcompiler.lib
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(DXGUID_LIBRARY dxguid.lib
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/Lib/*/um/x64"
            "$ENV{ProgramFiles}/Windows Kits/8.1/Lib/winv6.3/um/x64"
    )

    find_library(PIX_LIBRARY WinPixEventRuntime.lib
            HINTS
            "$ENV{ProgramFiles}/Windows Kits/10/bin/*/x64"
    )

    if(NOT D3D12_INCLUDE_DIR OR NOT D3D12_LIBRARY)
        message(FATAL_ERROR "DirectX 12 not found")
    endif()

    add_library(directx12 INTERFACE)
    target_include_directories(directx12 INTERFACE
            ${D3D12_INCLUDE_DIR}
            ${d3dx12_SOURCE_DIR}/include
            ${d3dx12_SOURCE_DIR}/include/directx
    )
    target_link_libraries(directx12 INTERFACE
            ${D3D12_LIBRARY}
            ${DXGI_LIBRARY}
            ${D3DCOMPILER_LIBRARY}
            ${DXGUID_LIBRARY}
            $<$<BOOL:${PIX_LIBRARY}>:${PIX_LIBRARY}>
    )
endif()

# Catch2
set(CATCH_BUILD_STATIC_LIBRARY ON CACHE INTERNAL "")
set(CATCH_BUILD_TESTING        OFF CACHE INTERNAL "")
FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.10
)
FetchContent_MakeAvailable(catch2)