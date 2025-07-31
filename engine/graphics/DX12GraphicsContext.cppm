module;
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <functional>
#include <string>

export module Graphics.DX12.GraphicsContext;

import Graphics;
import Graphics.Window;
import Graphics.DX12.Device;
import Graphics.DX12.SwapChain;
import Graphics.DX12.Renderer;
import Graphics.DX12.Pipeline;
import Graphics.DX12.Resource;
import Graphics.DX12.RenderGraph;
import Graphics.DX12.CommandList;
import Graphics.DX12.DescriptorHeap;
import Core.Types;
import Core.Assert;
import std;

using Microsoft::WRL::ComPtr;

struct ResourceHandle {
    enum Type { VertexBuffer, IndexBuffer, Pipeline } type;
    U32 index;
};

class DX12GraphicsContext : public IGraphicsContext {
private:
    Window* m_Window;
    std::unique_ptr<Renderer> m_Renderer;

    // Resource storage
    std::vector<std::unique_ptr<Buffer>> m_VertexBuffers;
    std::vector<std::unique_ptr<Buffer>> m_IndexBuffers;
    std::vector<std::unique_ptr<GraphicsPipeline>> m_Pipelines;
    std::vector<std::unique_ptr<RootSignature>> m_RootSignatures;

    // Current state
    U32 m_CurrentPipeline = INVALID_INDEX;
    U32 m_CurrentVertexBuffer = INVALID_INDEX;
    U32 m_CurrentIndexBuffer = INVALID_INDEX;
    bool m_InRenderPass = false;

    // Render graph data
    struct FramePassData {
        std::vector<std::function<void(ID3D12GraphicsCommandList*)>> commands;
        RenderPassInfo passInfo;
    };
    std::unique_ptr<FramePassData> m_CurrentPassData;

public:
    DX12GraphicsContext(Window& window, const GraphicsConfig& config) : m_Window{&window} {
        RendererConfig rendererConfig;
        rendererConfig.deviceConfig.enableDebugLayer = config.enableValidation;
        rendererConfig.swapChainConfig.bufferCount = config.frameBufferCount;

        m_Renderer = std::make_unique<Renderer>(window, rendererConfig);
        m_Renderer->GetSwapChain().SetVSync(config.enableVSync);
    }

    U32 CreateVertexBuffer(const void* data, U64 size) override {
        BufferDesc desc;
        desc.size = size;
        desc.usage = ResourceUsage::VertexBuffer;
        desc.cpuAccessible = true;

        auto buffer = std::make_unique<Buffer>(m_Renderer->GetDevice(), desc);
        buffer->UpdateData(data, size);

        U32 handle = static_cast<U32>(m_VertexBuffers.size());
        m_VertexBuffers.push_back(std::move(buffer));
        return handle;
    }

    U32 CreateIndexBuffer(const void* data, U64 size) override {
        BufferDesc desc;
        desc.size = size;
        desc.usage = ResourceUsage::IndexBuffer;
        desc.cpuAccessible = true;

        auto buffer = std::make_unique<Buffer>(m_Renderer->GetDevice(), desc);
        buffer->UpdateData(data, size);

        U32 handle = static_cast<U32>(m_IndexBuffers.size());
        m_IndexBuffers.push_back(std::move(buffer));
        return handle;
    }

    U32 CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) override {
        // Create root signature
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        auto rootSig = std::make_unique<RootSignature>(m_Renderer->GetDevice(), rootSigDesc);

        // Compile shaders
        GraphicsPipelineDesc pipelineDesc;

        for (const auto& shader : info.shaders) {
            std::string target;
            switch (shader.stage) {
                case ShaderStage::Vertex: target = "vs_5_0"; break;
                case ShaderStage::Pixel: target = "ps_5_0"; break;
                case ShaderStage::Geometry: target = "gs_5_0"; break;
                case ShaderStage::Hull: target = "hs_5_0"; break;
                case ShaderStage::Domain: target = "ds_5_0"; break;
                default: assert(false, "Invalid shader stage for graphics pipeline");
            }

            auto bytecode = CompileShader(shader.source, shader.entryPoint, target);

            switch (shader.stage) {
                case ShaderStage::Vertex: pipelineDesc.vertexShader = bytecode; break;
                case ShaderStage::Pixel: pipelineDesc.pixelShader = bytecode; break;
                case ShaderStage::Geometry: pipelineDesc.geometryShader = bytecode; break;
                case ShaderStage::Hull: pipelineDesc.hullShader = bytecode; break;
                case ShaderStage::Domain: pipelineDesc.domainShader = bytecode; break;
            }
        }

        // Build input layout
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        for (const auto& [name, attrOffset] : info.vertexAttributes) {
            DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT; // Default to float3
            if (attrOffset == 12) format = DXGI_FORMAT_R32G32B32A32_FLOAT; // float4 for color

            inputLayout.push_back({
                name.c_str(), 0, format, 0, attrOffset,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            });
        }
        pipelineDesc.inputLayout = std::move(inputLayout);

        // Set pipeline properties
        pipelineDesc.rootSignature = rootSig->GetRootSignature();
        pipelineDesc.primitiveTopology = ConvertTopology(info.topology);
        pipelineDesc.numRenderTargets = 1;
        pipelineDesc.rtvFormats[0] = m_Renderer->GetSwapChain().GetFormat();
        pipelineDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;

        if (!info.depthTest) {
            pipelineDesc.depthStencilState.DepthEnable = FALSE;
        }
        if (!info.depthWrite) {
            pipelineDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }

        auto pipeline = std::make_unique<GraphicsPipeline>(m_Renderer->GetDevice(), pipelineDesc);

        U32 handle = static_cast<U32>(m_Pipelines.size());
        m_Pipelines.push_back(std::move(pipeline));
        m_RootSignatures.push_back(std::move(rootSig));

        return handle;
    }

    void BeginFrame() override {
        m_Renderer->BeginFrame();
        m_CurrentPassData = std::make_unique<FramePassData>();

        // Reset render graph for new frame
        m_Renderer->ResetRenderGraph();
    }

    void EndFrame() override {
        // Compile render graph with accumulated passes
        m_Renderer->GetRenderGraph().Compile();

        // Execute render graph with accumulated commands
        m_Renderer->GetRenderGraph().Execute(m_Renderer->GetCurrentCommandList());
        m_Renderer->EndFrame();
        m_CurrentPassData.reset();
    }

    void BeginRenderPass(const RenderPassInfo& info) override {
        assert(!m_InRenderPass, "Already in render pass");
        m_InRenderPass = true;
        m_CurrentPassData->passInfo = info;
    }

    void EndRenderPass() override {
        assert(m_InRenderPass, "Not in render pass");
        m_InRenderPass = false;

        // Create a shared_ptr to transfer ownership to the lambda
        auto passData = std::make_shared<FramePassData>(std::move(*m_CurrentPassData));
        m_CurrentPassData = std::make_unique<FramePassData>();

        m_Renderer->GetRenderGraph().AddPass<FramePassData>(
            passData->passInfo.name,
            [](RenderGraphBuilder&, FramePassData&) {
                // No resource setup needed for simple forward pass
            },
            [this, passData](const RenderGraphResources&, CommandList& cmdList, const FramePassData&) {
                ExecuteRenderPass(cmdList, *passData);
            }
        );
    }

    void SetPipeline(U32 pipeline) override {
        m_CurrentPipeline = pipeline;
    }

    void SetVertexBuffer(U32 buffer) override {
        m_CurrentVertexBuffer = buffer;
    }

    void SetIndexBuffer(U32 buffer) override {
        m_CurrentIndexBuffer = buffer;
    }

    void Draw(U32 vertexCount, U32 instanceCount, U32 firstVertex, U32 firstInstance) override {
        assert(m_InRenderPass, "Must be in render pass");

        m_CurrentPassData->commands.push_back([=](ID3D12GraphicsCommandList* cmd) {
            cmd->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        });
    }

    void DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, S32 vertexOffset, U32 firstInstance) override {
        assert(m_InRenderPass, "Must be in render pass");

        m_CurrentPassData->commands.push_back([=](ID3D12GraphicsCommandList* cmd) {
            cmd->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        });
    }

    void OnResize(U32 width, U32 height) override {
        m_Window->UpdateDimensions(width, height);
        m_Renderer->Resize(*m_Window);
    }

    bool ShouldClose() const override {
        return m_Window->ShouldClose();
    }

private:
    void ExecuteRenderPass(CommandList& cmdList, const FramePassData& passData) {
        auto* cmd = cmdList.GetCommandList();

        // Set viewport and scissor
        D3D12_VIEWPORT viewport = {
            0.0f, 0.0f,
            static_cast<float>(m_Renderer->GetSwapChain().GetWidth()),
            static_cast<float>(m_Renderer->GetSwapChain().GetHeight()),
            0.0f, 1.0f
        };
        D3D12_RECT scissor = {
            0, 0,
            static_cast<LONG>(m_Renderer->GetSwapChain().GetWidth()),
            static_cast<LONG>(m_Renderer->GetSwapChain().GetHeight())
        };

        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);

        // Transition render target
        Resource rtResource(m_Renderer->GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, ResourceType::Texture2D);
        rtResource.SetTracked(true);
        cmdList.TransitionResource(rtResource, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Set render targets
        auto rtvHandle = m_Renderer->GetCurrentRTV();
        auto dsvHandle = m_Renderer->GetDSV();
        cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        // Clear if requested
        if (passData.passInfo.clearColor) {
            cmd->ClearRenderTargetView(rtvHandle, passData.passInfo.clearColorValue, 0, nullptr);
        }
        if (passData.passInfo.clearDepth) {
            cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, passData.passInfo.clearDepthValue, 0, 0, nullptr);
        }

        // Set current pipeline
        if (m_CurrentPipeline != INVALID_INDEX) {
            m_Pipelines[m_CurrentPipeline]->Bind(cmd);
            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }

        // Set vertex buffer
        if (m_CurrentVertexBuffer != INVALID_INDEX) {
            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = m_VertexBuffers[m_CurrentVertexBuffer]->GetGPUAddress();
            vbView.SizeInBytes = static_cast<UINT>(m_VertexBuffers[m_CurrentVertexBuffer]->GetDesc().size);
            vbView.StrideInBytes = sizeof(Vertex); // TODO: Get from pipeline info
            cmd->IASetVertexBuffers(0, 1, &vbView);
        }

        // Set index buffer
        if (m_CurrentIndexBuffer != INVALID_INDEX) {
            D3D12_INDEX_BUFFER_VIEW ibView;
            ibView.BufferLocation = m_IndexBuffers[m_CurrentIndexBuffer]->GetGPUAddress();
            ibView.SizeInBytes = static_cast<UINT>(m_IndexBuffers[m_CurrentIndexBuffer]->GetDesc().size);
            ibView.Format = DXGI_FORMAT_R32_UINT;
            cmd->IASetIndexBuffer(&ibView);
        }

        // Execute recorded commands
        for (const auto& command : passData.commands) {
            command(cmd);
        }

        // Transition back to present
        cmdList.TransitionResource(rtResource, D3D12_RESOURCE_STATE_PRESENT);
    }

    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertTopology(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::TriangleList:
            case PrimitiveTopology::TriangleStrip:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            case PrimitiveTopology::LineList:
            case PrimitiveTopology::LineStrip:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case PrimitiveTopology::PointList:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            default:
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }
};

export std::unique_ptr<IGraphicsContext> CreateDX12GraphicsContext(Window& window, const GraphicsConfig& config) {
    return std::make_unique<DX12GraphicsContext>(window, config);
}