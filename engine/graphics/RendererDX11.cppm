module;

#ifdef VOXEL_USE_DX11

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

export module Graphics.RendererDX11;

import Graphics.Window;
import Core.Assert;
import Core.Types;
import Core.Log;

using Microsoft::WRL::ComPtr;

export class RendererDX11 {
private:
    ComPtr<ID3D11Device> m_Device;
    ComPtr<ID3D11DeviceContext> m_Context;
    ComPtr<IDXGISwapChain> m_SwapChain;
    ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
    ComPtr<ID3D11DepthStencilView> m_DepthStencilView;
    ComPtr<ID3D11Texture2D> m_DepthBuffer;

    Window* m_Window;
    D3D11_VIEWPORT m_Viewport;

public:
    explicit RendererDX11(Window* window) : m_Window{window} {
        CreateDevice();
        CreateSwapChain();
        CreateRenderTarget();
        SetupViewport();
    }

    void BeginFrame() {
        // Clear render target
        constexpr float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
        m_Context->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);
        m_Context->ClearDepthStencilView(
            m_DepthStencilView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
            1.0f,
            0
        );

        // Set render target
        m_Context->OMSetRenderTargets(
            1,
            m_RenderTargetView.GetAddressOf(),
            m_DepthStencilView.Get()
        );
        m_Context->RSSetViewports(1, &m_Viewport);
    }

    void EndFrame() const {
        m_SwapChain->Present(1, 0); // TODO Vsync
    }

    void OnResize(U32 width, U32 height) {
        if (!m_Device) return;

        m_Context->OMSetRenderTargets(0, nullptr, nullptr);
        m_RenderTargetView.Reset();
        m_DepthStencilView.Reset();
        m_DepthBuffer.Reset();

        HRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            Logger::Error("Failed to resize swap schain buffers");
        }

        CreateRenderTarget();
        SetupViewport();
    }

    [[nodiscard]] ID3D11Device* GetDevice() const { return m_Device.Get(); }
    [[nodiscard]] ID3D11DeviceContext* GetContext() const { return m_Context.Get(); }

private:
    void CreateDevice() {
        UINT createDeviceFlags = 0;
#if _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            &m_Device,
            &featureLevel,
            &m_Context
        );

        assert(!FAILED(hr), "Failed to create D3D11 device");

        Logger::Info("Created D3D11 device with feature level: {}",
            (featureLevel == D3D_FEATURE_LEVEL_11_1) ? "11.1" : "11.0"
        );
    }

    void CreateSwapChain() {
        ComPtr<IDXGIDevice> dxgiDevice;
        m_Device.As(&dxgiDevice);

        ComPtr<IDXGIAdapter> dxgiAdapter;
        dxgiDevice->GetAdapter(&dxgiAdapter);

        ComPtr<IDXGIFactory> dxgiFactory;
        dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChainDesc.BufferDesc.Width = m_Window->GetWidth();
        swapChainDesc.BufferDesc.Height = m_Window->GetHeight();
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.OutputWindow = m_Window->GetWin32Handle();
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        HRESULT hr = dxgiFactory->CreateSwapChain(m_Device.Get(), &swapChainDesc, &m_SwapChain);
        assert(!FAILED(hr), "Failed to create swap chain");

        // Disable Alt+Enter fullscreen
        dxgiFactory->MakeWindowAssociation(m_Window->GetWin32Handle(), DXGI_MWA_NO_ALT_ENTER);
    }

    void CreateRenderTarget() {
        ComPtr<ID3D11Texture2D> backBuffer;
        m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTargetView);

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = m_Window->GetWidth();
        depthDesc.Height = m_Window->GetHeight();
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        m_Device->CreateTexture2D(&depthDesc, nullptr, &m_DepthBuffer);
        m_Device->CreateDepthStencilView(m_DepthBuffer.Get(), nullptr, &m_DepthStencilView);
    }

    void SetupViewport() {
        m_Viewport.TopLeftX = 0.0f;
        m_Viewport.TopLeftY = 0.0f;
        m_Viewport.Width = static_cast<float>(m_Window->GetWidth());
        m_Viewport.Height = static_cast<float>(m_Window->GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }
};

#endif
