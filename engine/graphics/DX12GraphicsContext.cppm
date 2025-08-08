module;
#include <d3d12.h>
#include <d3dx12.h>
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
import Graphics.DX12.ShaderManager;
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
    Window *m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ShaderManager> m_ShaderManager;
    std::vector<std::unique_ptr<Buffer> > m_VertexBuffers;
    std::vector<std::unique_ptr<Buffer> > m_IndexBuffers;
    std::vector<std::unique_ptr<Buffer> > m_ConstantBuffers;
    std::vector<std::unique_ptr<GraphicsPipeline> > m_Pipelines;
    std::vector<std::unique_ptr<RootSignature> > m_RootSignatures;
    struct TextureEntry { std::unique_ptr<Texture> tex; DescriptorHandle srv; U32 width; U32 height; };
    std::vector<TextureEntry> m_Textures;
    U32 m_CurrentPipeline = INVALID_INDEX;
    U32 m_CurrentVertexBuffer = INVALID_INDEX;
    U32 m_CurrentIndexBuffer = INVALID_INDEX;
    bool m_InRenderPass = false;
    struct FramePassData { std::vector<std::function<void(ID3D12GraphicsCommandList *)> > commands; RenderPassInfo passInfo; };
    std::unique_ptr<FramePassData> m_CurrentPassData;

public:
    DX12GraphicsContext(Window &window, const GraphicsConfig &config) : m_Window{&window} {
        RendererConfig rendererConfig;
        rendererConfig.deviceConfig.enableDebugLayer = config.enableValidation;
        rendererConfig.swapChainConfig.bufferCount = config.frameBufferCount;
        m_Renderer = std::make_unique<Renderer>(window, rendererConfig);
        m_Renderer->GetSwapChain().SetVSync(config.enableVSync);
        m_ShaderManager = std::make_unique<ShaderManager>("shaders");
    }

    U32 CreateVertexBuffer(const void *data, U64 size) override {
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

    U32 CreateIndexBuffer(const void *data, U64 size) override {
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

    U32 CreateConstantBuffer(U64 size) override {
        U64 alignedSize = (size + 255) & ~255;
        BufferDesc desc;
        desc.size = alignedSize;
        desc.usage = ResourceUsage::ConstantBuffer;
        desc.cpuAccessible = true;
        auto buffer = std::make_unique<Buffer>(m_Renderer->GetDevice(), desc);
        U32 handle = static_cast<U32>(m_ConstantBuffers.size());
        m_ConstantBuffers.push_back(std::move(buffer));
        return handle;
    }

    void UpdateConstantBuffer(U32 buffer, const void *data, U64 size) override {
        if (buffer >= m_ConstantBuffers.size()) return;
        m_ConstantBuffers[buffer]->UpdateData(data, size);
    }

    U32 CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &info) override {
        std::vector<D3D12_ROOT_PARAMETER> rootParams;
        D3D12_ROOT_PARAMETER cameraParam = {};
        cameraParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        cameraParam.Descriptor.ShaderRegister = 0;
        cameraParam.Descriptor.RegisterSpace = 0;
        cameraParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams.push_back(cameraParam);
        D3D12_ROOT_PARAMETER objectParam = {};
        objectParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        objectParam.Descriptor.ShaderRegister = 1;
        objectParam.Descriptor.RegisterSpace = 0;
        objectParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams.push_back(objectParam);
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = 0;
        D3D12_ROOT_DESCRIPTOR_TABLE table = {};
        table.NumDescriptorRanges = 1;
        table.pDescriptorRanges = &range;
        D3D12_ROOT_PARAMETER srvTable = {};
        srvTable.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        srvTable.DescriptorTable = table;
        srvTable.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams.push_back(srvTable);
        D3D12_ROOT_PARAMETER atlasParam = {};
        atlasParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        atlasParam.Descriptor.ShaderRegister = 2;
        atlasParam.Descriptor.RegisterSpace = 0;
        atlasParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams.push_back(atlasParam);
        D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.MipLODBias = 0.0f;
        staticSampler.MaxAnisotropy = 1;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        staticSampler.MinLOD = 0.0f;
        staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
        rootSigDesc.pParameters = rootParams.data();
        rootSigDesc.NumStaticSamplers = 1;
        rootSigDesc.pStaticSamplers = &staticSampler;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        auto rootSig = std::make_unique<RootSignature>(m_Renderer->GetDevice(), rootSigDesc);
        GraphicsPipelineDesc pipelineDesc;
        for (const auto &shader: info.shaders) {
            std::string target;
            switch (shader.stage) {
                case ShaderStage::Vertex: target = "vs_5_0"; break;
                case ShaderStage::Pixel: target = "ps_5_0"; break;
                case ShaderStage::Geometry: target = "gs_5_0"; break;
                case ShaderStage::Hull: target = "hs_5_0"; break;
                case ShaderStage::Domain: target = "ds_5_0"; break;
                default: assert(false, "Invalid shader stage for graphics pipeline");
            }
            ShaderBytecode bytecode;
            if (shader.source.find('\n') == std::string::npos && shader.source.ends_with(".hlsl")) {
                bytecode = m_ShaderManager->LoadShader(shader.source, shader.entryPoint, target);
            } else {
                bytecode = m_ShaderManager->CompileShaderFromSource(shader.source, "inline_shader", shader.entryPoint, target);
            }
            switch (shader.stage) {
                case ShaderStage::Vertex: pipelineDesc.vertexShader = bytecode; break;
                case ShaderStage::Pixel: pipelineDesc.pixelShader = bytecode; break;
                case ShaderStage::Geometry: pipelineDesc.geometryShader = bytecode; break;
                case ShaderStage::Hull: pipelineDesc.hullShader = bytecode; break;
                case ShaderStage::Domain: pipelineDesc.domainShader = bytecode; break;
                default: break;
            }
        }
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        for (const auto &[name, attrOffset]: info.vertexAttributes) {
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            if (name == "POSITION") format = DXGI_FORMAT_R32G32B32_FLOAT;
            else if (name == "NORMAL") format = DXGI_FORMAT_R32G32B32_FLOAT;
            else if (name == "TEXCOORD") format = DXGI_FORMAT_R32G32_FLOAT;
            else if (name == "COLOR") format = DXGI_FORMAT_R32_UINT;
            else format = DXGI_FORMAT_R32G32B32_FLOAT;
            inputLayout.push_back({name.c_str(), 0, format, 0, attrOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        }
        pipelineDesc.inputLayout = std::move(inputLayout);
        pipelineDesc.rootSignature = rootSig->GetRootSignature();
        pipelineDesc.primitiveTopology = ConvertTopology(info.topology);
        pipelineDesc.numRenderTargets = 1;
        pipelineDesc.rtvFormats[0] = m_Renderer->GetSwapChain().GetFormat();
        pipelineDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
        if (!info.depthTest) pipelineDesc.depthStencilState.DepthEnable = FALSE;
        if (!info.depthWrite) pipelineDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        auto pipeline = std::make_unique<GraphicsPipeline>(m_Renderer->GetDevice(), pipelineDesc);
        U32 handle = static_cast<U32>(m_Pipelines.size());
        m_Pipelines.push_back(std::move(pipeline));
        m_RootSignatures.push_back(std::move(rootSig));
        return handle;
    }

    void BeginFrame() override {
        m_Renderer->BeginFrame();
        m_CurrentPassData = std::make_unique<FramePassData>();
        m_Renderer->ResetRenderGraph();
    }

    void EndFrame() override {
        m_Renderer->GetRenderGraph().Compile();
        m_Renderer->GetRenderGraph().Execute(m_Renderer->GetCurrentCommandList());
        m_Renderer->EndFrame();
        m_CurrentPassData.reset();
    }

    void BeginRenderPass(const RenderPassInfo &info) override {
        assert(!m_InRenderPass, "Already in render pass");
        m_InRenderPass = true;
        m_CurrentPassData->passInfo = info;
    }

    void EndRenderPass() override {
        assert(m_InRenderPass, "Not in render pass");
        m_InRenderPass = false;
        auto passData = std::make_shared<FramePassData>(std::move(*m_CurrentPassData));
        m_CurrentPassData = std::make_unique<FramePassData>();
        m_Renderer->GetRenderGraph().AddPass<FramePassData>(
            passData->passInfo.name,
            [](RenderGraphBuilder &, FramePassData &) {},
            [this, passData](const RenderGraphResources &, CommandList &cmdList, const FramePassData &) {
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
        const U32 pso = m_CurrentPipeline;
        const U32 vb = m_CurrentVertexBuffer;
        const U32 ib = m_CurrentIndexBuffer;
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (pso != INVALID_INDEX) {
                m_Pipelines[pso]->Bind(cmd);
                cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            }
            if (vb != INVALID_INDEX) {
                D3D12_VERTEX_BUFFER_VIEW vbView{};
                vbView.BufferLocation = m_VertexBuffers[vb]->GetGPUAddress();
                vbView.SizeInBytes = (UINT) m_VertexBuffers[vb]->GetDesc().size;
                vbView.StrideInBytes = sizeof(Vertex);
                cmd->IASetVertexBuffers(0, 1, &vbView);
            }
            if (ib != INVALID_INDEX) {
                D3D12_INDEX_BUFFER_VIEW ibView{};
                ibView.BufferLocation = m_IndexBuffers[ib]->GetGPUAddress();
                ibView.SizeInBytes = (UINT) m_IndexBuffers[ib]->GetDesc().size;
                ibView.Format = DXGI_FORMAT_R32_UINT;
                cmd->IASetIndexBuffer(&ibView);
            }
            cmd->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        });
    }

    void DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, S32 vertexOffset, U32 firstInstance) override {
        assert(m_InRenderPass, "Must be in render pass");
        m_CurrentPassData->commands.push_back([=](ID3D12GraphicsCommandList *cmd) {
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

    ShaderManager *GetShaderManager() { return m_ShaderManager.get(); }

    U32 CreateTexture2D(const void *rgba8, U32 width, U32 height, U32 mipLevels = 1) override {
        TextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.mipLevels = mipLevels;
        desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.usage = ResourceUsage::ShaderResource;
        auto tex = std::make_unique<Texture>(m_Renderer->GetDevice(), desc);
        ID3D12Resource *resource = tex->GetResource();
        D3D12_RESOURCE_DESC rd = resource->GetDesc();
        U64 uploadSize = 0;
        m_Renderer->GetDevice().GetDevice()->GetCopyableFootprints(&rd, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = uploadSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc = {1, 0};
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        ComPtr<ID3D12Resource> upload;
        assert(SUCCEEDED(m_Renderer->GetDevice().GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))), "Failed to create upload buffer");
        D3D12_SUBRESOURCE_DATA sub{};
        sub.pData = rgba8;
        sub.RowPitch = static_cast<LONG_PTR>(width) * 4;
        sub.SlicePitch = sub.RowPitch * height;
        auto *cmd = m_Renderer->GetCurrentCommandList().GetCommandList();
        D3D12_RESOURCE_BARRIER b0{};
        b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b0.Transition.pResource = resource;
        b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        cmd->ResourceBarrier(1, &b0);
        UpdateSubresources(cmd, resource, upload.Get(), 0, 0, 1, &sub);
        m_Renderer->GetCurrentCommandList().KeepAlive(upload);
        D3D12_RESOURCE_BARRIER b1{};
        b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b1.Transition.pResource = resource;
        b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b1.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &b1);
        auto handle = m_Renderer->GetCbvSrvUavHeap()->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = desc.format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = desc.mipLevels;
        m_Renderer->GetDevice().GetDevice()->CreateShaderResourceView(resource, &srv, handle.cpuHandle);
        TextureEntry entry{};
        entry.tex = std::move(tex);
        entry.srv = handle;
        entry.width = width;
        entry.height = height;
        U32 id = static_cast<U32>(m_Textures.size());
        m_Textures.push_back(std::move(entry));
        return id;
    }

    void SetTexture(U32 texture, U32 slot) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (texture >= m_Textures.size()) return;
        auto gpu = m_Textures[texture].srv.gpuHandle;
        m_CurrentPassData->commands.push_back([=](ID3D12GraphicsCommandList *cmd) {
            cmd->SetGraphicsRootDescriptorTable(2, gpu);
        });
    }

    void SetConstantBuffer(U32 buffer, U32 slot) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (buffer >= m_ConstantBuffers.size()) return;
        auto addr = m_ConstantBuffers[buffer]->GetGPUAddress();
        m_CurrentPassData->commands.push_back([=](ID3D12GraphicsCommandList *cmd) {
            if (slot == 0) cmd->SetGraphicsRootConstantBufferView(0, addr);
            else if (slot == 1) cmd->SetGraphicsRootConstantBufferView(1, addr);
            else if (slot == 2) cmd->SetGraphicsRootConstantBufferView(3, addr);
        });
    }

private:
    void ExecuteRenderPass(CommandList &cmdList, const FramePassData &passData) const {
        auto *cmd = cmdList.GetCommandList();
        D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(m_Renderer->GetSwapChain().GetWidth()), static_cast<float>(m_Renderer->GetSwapChain().GetHeight()), 0.0f, 1.0f};
        D3D12_RECT scissor = {0, 0, static_cast<LONG>(m_Renderer->GetSwapChain().GetWidth()), static_cast<LONG>(m_Renderer->GetSwapChain().GetHeight())};
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);
        ID3D12DescriptorHeap *heaps[] = {m_Renderer->GetCbvSrvUavHeap()->GetCurrentHeap()};
        cmd->SetDescriptorHeaps(1, heaps);
        Resource rtResource(m_Renderer->GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, ResourceType::Texture2D);
        rtResource.SetTracked(true);
        cmdList.TransitionResource(rtResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto rtvHandle = m_Renderer->GetCurrentRTV();
        auto dsvHandle = m_Renderer->GetDSV();
        cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        if (passData.passInfo.clearColor) { cmd->ClearRenderTargetView(rtvHandle, passData.passInfo.clearColorValue, 0, nullptr); }
        if (passData.passInfo.clearDepth) { cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, passData.passInfo.clearDepthValue, 0, 0, nullptr); }
        if (m_CurrentPipeline != INVALID_INDEX) { m_Pipelines[m_CurrentPipeline]->Bind(cmd); cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); }
        if (m_CurrentVertexBuffer != INVALID_INDEX) {
            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = m_VertexBuffers[m_CurrentVertexBuffer]->GetGPUAddress();
            vbView.SizeInBytes = static_cast<UINT>(m_VertexBuffers[m_CurrentVertexBuffer]->GetDesc().size);
            vbView.StrideInBytes = sizeof(Vertex);
            cmd->IASetVertexBuffers(0, 1, &vbView);
        }
        if (m_CurrentIndexBuffer != INVALID_INDEX) {
            D3D12_INDEX_BUFFER_VIEW ibView;
            ibView.BufferLocation = m_IndexBuffers[m_CurrentIndexBuffer]->GetGPUAddress();
            ibView.SizeInBytes = static_cast<UINT>(m_IndexBuffers[m_CurrentIndexBuffer]->GetDesc().size);
            ibView.Format = DXGI_FORMAT_R32_UINT;
            cmd->IASetIndexBuffer(&ibView);
        }
        for (const auto &command: passData.commands) command(cmd);
        cmdList.TransitionResource(rtResource, D3D12_RESOURCE_STATE_PRESENT);
    }

    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertTopology(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::TriangleList:
            case PrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            case PrimitiveTopology::LineList:
            case PrimitiveTopology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case PrimitiveTopology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }
};

export std::unique_ptr<IGraphicsContext> CreateDX12GraphicsContext(Window &window, const GraphicsConfig &config) {
    return std::make_unique<DX12GraphicsContext>(window, config);
}
