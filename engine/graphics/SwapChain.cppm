module;
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.SwapChain;

import Core.Assert;
import Core.Types;
import Graphics.Window;
import Graphics.DX12.Device;

using Microsoft::WRL::ComPtr;

export struct SwapChainConfig {
    U32 bufferCount = 3;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool allowTearing = true;
};

export class SwapChain {
private:
    ComPtr<IDXGISwapChain4> m_SwapChain;
    Vector<ComPtr<ID3D12Resource>> m_BackBuffers;
    SwapChainConfig m_Config;
    U32 m_CurrentBackBuffer = 0;
    U32 m_Width;
    U32 m_Height;
    bool m_VSyncEnabled = true;

public:
    SwapChain(Device& device, Window& window, const SwapChainConfig& config)
        : m_Config{config}, m_Width{window.GetWidth()}, m_Height{window.GetHeight()} {

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = m_Width;
        swapChainDesc.Height = m_Height;
        swapChainDesc.Format = config.format;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = {1, 0};
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = config.bufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_NONE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = config.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        assert(SUCCEEDED(device.GetFactory()->CreateSwapChainForHwnd(
            device.GetDirectQueue(),
            window.GetWin32Handle(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        )), "Failed to create swap chain");

        assert(SUCCEEDED(swapChain1.As(&m_SwapChain)), "Failed to get IDXGISwapChain4");

        // Disable fullscreen transitions
        assert(SUCCEEDED(device.GetFactory()->MakeWindowAssociation(window.GetWin32Handle(), DXGI_MWA_NO_ALT_ENTER)), "Failed to disable Alt+Enter");

        UpdateBackBuffers(device.GetDevice());
    }

    void Present() {
        UINT presentFlags = (!m_VSyncEnabled && m_Config.allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        UINT syncInterval = m_VSyncEnabled ? 1 : 0;

        assert(SUCCEEDED(m_SwapChain->Present(syncInterval, presentFlags)), "Failed to present");
        m_CurrentBackBuffer = m_SwapChain->GetCurrentBackBufferIndex();
    }

    void Resize(Device& device, U32 width, U32 height) {
        if (width == m_Width && height == m_Height) return;

        m_Width = width;
        m_Height = height;

        // Release existing back buffers
        m_BackBuffers.clear();

        DXGI_SWAP_CHAIN_DESC1 desc;
        m_SwapChain->GetDesc1(&desc);

        assert(SUCCEEDED(m_SwapChain->ResizeBuffers(
            m_Config.bufferCount,
            width,
            height,
            desc.Format,
            desc.Flags
        )), "Failed to resize swap chain");

        UpdateBackBuffers(device.GetDevice());
    }

    [[nodiscard]] ID3D12Resource* GetCurrentBackBuffer() const {
        return m_BackBuffers[m_CurrentBackBuffer].Get();
    }

    [[nodiscard]] ID3D12Resource* GetBackBuffer(U32 index) const {
        assert(index < m_Config.bufferCount, "Invalid back buffer index");
        return m_BackBuffers[index].Get();
    }

    [[nodiscard]] U32 GetCurrentBackBufferIndex() const { return m_CurrentBackBuffer; }
    [[nodiscard]] U32 GetBufferCount() const { return m_Config.bufferCount; }
    [[nodiscard]] DXGI_FORMAT GetFormat() const { return m_Config.format; }
    [[nodiscard]] U32 GetWidth() const { return m_Width; }
    [[nodiscard]] U32 GetHeight() const { return m_Height; }

    void SetVSync(bool enabled) { m_VSyncEnabled = enabled; }

private:
    void UpdateBackBuffers(ID3D12Device* device) {
        m_BackBuffers.resize(m_Config.bufferCount);

        for (U32 i = 0; i < m_Config.bufferCount; ++i) {
            assert(SUCCEEDED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i]))), "Failed to get swap chain buffer");
        }

        m_CurrentBackBuffer = m_SwapChain->GetCurrentBackBufferIndex();
    }
};