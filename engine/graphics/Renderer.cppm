export module Graphics.Renderer;

import Graphics.Window;
import Core.Types;
import std;

#ifdef VOXEL_USE_DX11
import Graphics.RendererDX11;
#else defined(VOXEL_USE_VULKAN)
import Graphics.RendererVulkan;
#endif

export class Renderer {
private:
    Window* m_Window;

#ifdef VOXEL_USE_DX11
    UniquePtr<RendererDX11> m_Backend;
#else defined(VOXEL_USE_VULKAN)
    UniquePtr<RendererVulkan> m_Backend;
#endif

public:
    explicit Renderer(Window* window) : m_Window{window} {
#ifdef VOXEL_USE_DX11
        m_Backend = std::make_unique<RendererDX11>(window);
#else defined(VOXEL_USE_VULKAN)
        m_Backend = std::make_unique<RendererVulkan>(window);
#endif
    }

    void BeginFrame() const {
        m_Backend->BeginFrame();
    }

    void EndFrame() const {
        m_Backend->EndFrame();
    }

    void OnResize(U32 width, U32 height) const {
        m_Backend->OnResize(width, height);
    }

#ifdef VOXEL_USE_DX11
    [[nodiscard]] RendererDX11* GetDX11Backend() const { return m_Backend.get(); }
#else defined(VOXEL_USE_VULKAN)
    [[nodiscard]] RendererVulkan* GetVulkanBackend() const { return m_Backend.get(); }
#endif
};