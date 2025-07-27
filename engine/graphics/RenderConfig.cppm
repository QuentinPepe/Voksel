export module Graphics.RenderConfig;

#ifdef VOXEL_USE_DX11
#define VOXEL_RENDER_DX11
#elif defined(VOXEL_USE_VULKAN)
#define VOXEL_RENDER_VULKAN
#else
#error "No render backend selected"
#endif

export {
#ifdef VOXEL_RENDER_DX11
    constexpr bool USE_DX11 = true;
    constexpr bool USE_VULKAN = false;
#else
    constexpr bool USE_DX11 = false;
    constexpr bool USE_VULKAN = true;
#endif
}