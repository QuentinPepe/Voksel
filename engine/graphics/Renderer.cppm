module;
#include <d3d12.h>
#include <d3dx12.h>

export module Graphics.DX12.Renderer;

import Core.Types;
import Core.Assert;
import Core.Log;
import Graphics.Window;
import Graphics.DX12.Device;
import Graphics.DX12.SwapChain;
import Graphics.DX12.CommandList;
import Graphics.DX12.Fence;
import Graphics.DX12.Resource;
import Graphics.DX12.DescriptorHeap;
import Graphics.DX12.Pipeline;
import Graphics.DX12.RenderGraph;
import std;

export struct RendererConfig {
    DeviceConfig deviceConfig;
    SwapChainConfig swapChainConfig;
};

export class Renderer {
private:
    static constexpr U32 FRAME_COUNT = 3;

    Device m_Device;
    SwapChain m_SwapChain;

    struct FrameData {
        std::unique_ptr<CommandList> commandList;
        std::unique_ptr<Fence> fence;
        U64 fenceValue = 0;
    };

    Array<FrameData, FRAME_COUNT> m_FrameData;
    U32 m_CurrentFrame = 0;

    std::unique_ptr<DescriptorHeap> m_RtvHeap;
    std::unique_ptr<DescriptorHeap> m_DsvHeap;
    std::unique_ptr<DynamicDescriptorHeap> m_CbvSrvUavHeap;

    std::unique_ptr<Texture> m_DepthBuffer;
    DescriptorHandle m_DepthHandle;

    Array<DescriptorHandle, FRAME_COUNT> m_RtvHandles;

    std::unique_ptr<RenderGraph> m_RenderGraph;

public:
    Renderer(Window& window, const RendererConfig& config)
        : m_Device{config.deviceConfig}
        , m_SwapChain{m_Device, window, config.swapChainConfig} {

        // Create descriptor heaps
        m_RtvHeap = std::make_unique<DescriptorHeap>(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
        m_DsvHeap = std::make_unique<DescriptorHeap>(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 8);
        m_CbvSrvUavHeap = std::make_unique<DynamicDescriptorHeap>(m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Create frame data
        for (U32 i = 0; i < FRAME_COUNT; ++i) {
            m_FrameData[i].commandList = std::make_unique<CommandList>(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
            m_FrameData[i].fence = std::make_unique<Fence>(m_Device);
        }

        // Create render targets
        CreateRenderTargets();

        // Create render graph
        m_RenderGraph = std::make_unique<RenderGraph>(m_Device);

        Logger::Info("Renderer initialized");
    }

    void BeginFrame() {
        auto& frameData = m_FrameData[m_CurrentFrame];

        // Wait for previous frame using this data to complete
        frameData.fence->WaitCPU(frameData.fenceValue);

        // Reset allocators and lists
        frameData.commandList->Begin();
    }

    void EndFrame() {
        auto& frameData = m_FrameData[m_CurrentFrame];

        // Execute command list
        frameData.commandList->End();

        ID3D12CommandList* cmdLists[] = { frameData.commandList->GetCommandList() };
        m_Device.GetDirectQueue()->ExecuteCommandLists(1, cmdLists);

        m_SwapChain.Present();

        // Signal fence
        frameData.fenceValue = frameData.fence->Signal(m_Device.GetDirectQueue());

        // Move to next frame
        m_CurrentFrame = (m_CurrentFrame + 1) % FRAME_COUNT;
    }

    void Resize(Window& window) {
        // Wait for all frames to complete
        for (auto& frame : m_FrameData) {
            frame.fence->WaitCPU(frame.fenceValue);
        }

        // Release existing resources
        m_DepthBuffer.reset();

        // Resize swap chain
        m_SwapChain.Resize(m_Device, window.GetWidth(), window.GetHeight());

        // Recreate render targets
        CreateRenderTargets();
    }

    void ResetRenderGraph() {
        m_RenderGraph = std::make_unique<RenderGraph>(m_Device);
    }

    [[nodiscard]] Device& GetDevice() { return m_Device; }
    [[nodiscard]] SwapChain& GetSwapChain() { return m_SwapChain; }
    [[nodiscard]] CommandList& GetCurrentCommandList() { return *m_FrameData[m_CurrentFrame].commandList; }
    [[nodiscard]] RenderGraph& GetRenderGraph() const { return *m_RenderGraph; }
    [[nodiscard]] ID3D12Resource* GetCurrentRenderTarget() const { return m_SwapChain.GetCurrentBackBuffer(); }
    [[nodiscard]] ID3D12Resource* GetDepthBuffer() const { return m_DepthBuffer->GetResource(); }
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() { return m_RtvHandles[m_SwapChain.GetCurrentBackBufferIndex()].cpuHandle; }
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DepthHandle.cpuHandle; }

private:
    void CreateRenderTargets() {
        // Create RTVs for swap chain buffers
        for (U32 i = 0; i < m_SwapChain.GetBufferCount(); ++i) {
            m_RtvHandles[i] = m_RtvHeap->Allocate();

            m_Device.GetDevice()->CreateRenderTargetView(
                m_SwapChain.GetBackBuffer(i),
                nullptr,
                m_RtvHandles[i].cpuHandle
            );
        }

        // Create depth buffer
        TextureDesc depthDesc;
        depthDesc.width = m_SwapChain.GetWidth();
        depthDesc.height = m_SwapChain.GetHeight();
        depthDesc.format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.usage = ResourceUsage::DepthStencil;

        m_DepthBuffer = std::make_unique<Texture>(m_Device, depthDesc);

        // Create DSV
        m_DepthHandle = m_DsvHeap->Allocate();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        m_Device.GetDevice()->CreateDepthStencilView(
            m_DepthBuffer->GetResource(),
            &dsvDesc,
            m_DepthHandle.cpuHandle
        );
    }
};