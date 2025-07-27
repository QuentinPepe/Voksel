export module Graphics.RenderConfig;

export {
#ifdef VOXEL_RENDER_DX11
    constexpr bool USE_DX11 = true;
    constexpr bool USE_VULKAN = false;
#else
    constexpr bool USE_DX11 = false;
    constexpr bool USE_VULKAN = true;
#endif
}