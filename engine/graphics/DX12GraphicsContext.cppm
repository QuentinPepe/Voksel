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
#include <mutex>

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
    std::unique_ptr<Renderer> m_Renderer;
    Window *m_Window;
    std::unique_ptr<ShaderManager> m_ShaderManager;
    std::vector<std::unique_ptr<Buffer> > m_VertexBuffers;
    std::vector<std::unique_ptr<Buffer> > m_IndexBuffers;
    std::vector<std::unique_ptr<Buffer> > m_ConstantBuffers;
    std::vector<std::unique_ptr<GraphicsPipeline> > m_Pipelines;
    std::vector<std::unique_ptr<RootSignature> > m_RootSignatures;
    std::vector<D3D_PRIMITIVE_TOPOLOGY> m_PipelineTopologies;
    std::vector<UINT> m_PipelineVertexStrides;

    struct TextureEntry {
        std::unique_ptr<Texture> tex;
        DescriptorHandle srv;
        U32 width;
        U32 height;
    };

    std::vector<TextureEntry> m_Textures;
    bool m_InRenderPass = false;

    struct FramePassData {
        std::vector<std::function<void(ID3D12GraphicsCommandList *)> > commands;
        RenderPassInfo passInfo;
    };

    std::unique_ptr<FramePassData> m_CurrentPassData;
    std::mutex m_CmdMutex;
    U32 m_CurrentPipelineIndex{UINT_MAX};

public:
    DX12GraphicsContext(Window &window, const GraphicsConfig &config) : m_Window{&window} {
        RendererConfig rendererConfig{};
        rendererConfig.deviceConfig.enableDebugLayer = config.enableValidation;
        rendererConfig.swapChainConfig.bufferCount = config.frameBufferCount;
        m_Renderer = std::make_unique<Renderer>(window, rendererConfig);
        m_Renderer->GetSwapChain().SetVSync(config.enableVSync);
        m_ShaderManager = std::make_unique<ShaderManager>("shaders");
    }

    ~DX12GraphicsContext() {
        if (m_Renderer) { m_Renderer->WaitIdle(); }
        m_CurrentPassData.reset();
        m_RootSignatures.clear();
        m_Pipelines.clear();
        m_ConstantBuffers.clear();
        m_IndexBuffers.clear();
        m_VertexBuffers.clear();
        m_Textures.clear();
        m_ShaderManager.reset();
    }

    U32 CreateVertexBuffer(const void *data, U64 size) override {
        BufferDesc desc{};
        desc.size = size;
        desc.usage = ResourceUsage::VertexBuffer | ResourceUsage::CopyDest;
        desc.cpuAccessible = false;
        auto buffer{std::make_unique<Buffer>(m_Renderer->GetDevice(), desc)};

        if (data && size) {
            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = size;
            rd.Height = 1;
            rd.DepthOrArraySize = 1;
            rd.MipLevels = 1;
            rd.Format = DXGI_FORMAT_UNKNOWN;
            rd.SampleDesc = {1, 0};
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            rd.Flags = D3D12_RESOURCE_FLAG_NONE;

            ComPtr<ID3D12Resource> upload{};
            assert(SUCCEEDED(m_Renderer->GetDevice().GetDevice()->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))), "Failed to create upload buffer");

            void *mapped{};
            D3D12_RANGE range{0, 0};
            assert(SUCCEEDED(upload->Map(0, &range, &mapped)), "Map failed");
            std::memcpy(mapped, data, static_cast<size_t>(size));
            upload->Unmap(0, nullptr);

            auto &curCL{m_Renderer->GetCurrentCommandList()};
            if (curCL.IsRecording()) {
                auto *cmd{curCL.GetCommandList()};
                D3D12_RESOURCE_BARRIER b0{};
                b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b0.Transition.pResource = buffer->GetResource();
                b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                cmd->ResourceBarrier(1, &b0);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

                cmd->CopyBufferRegion(buffer->GetResource(), 0, upload.Get(), 0, size);

                D3D12_RESOURCE_BARRIER b1{};
                b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b1.Transition.pResource = buffer->GetResource();
                b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                b1.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                cmd->ResourceBarrier(1, &b1);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

                curCL.KeepAlive(upload);
            } else {
                ComPtr<ID3D12CommandAllocator> alloc{};
                ComPtr<ID3D12GraphicsCommandList> cmd{};
                auto *dev{m_Renderer->GetDevice().GetDevice()};
                auto *q{m_Renderer->GetDevice().GetDirectQueue()};
                assert(
                    SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))),
                    "alloc");
                assert(SUCCEEDED(
                    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)
                    )), "cmdlist");

                D3D12_RESOURCE_BARRIER b0{};
                b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b0.Transition.pResource = buffer->GetResource();
                b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                cmd->ResourceBarrier(1, &b0);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

                cmd->CopyBufferRegion(buffer->GetResource(), 0, upload.Get(), 0, size);

                D3D12_RESOURCE_BARRIER b1{};
                b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b1.Transition.pResource = buffer->GetResource();
                b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                b1.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                cmd->ResourceBarrier(1, &b1);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

                assert(SUCCEEDED(cmd->Close()), "close");
                ID3D12CommandList *lists[]{cmd.Get()};
                q->ExecuteCommandLists(1, lists);

                ComPtr<ID3D12Fence> fence{};
                assert(SUCCEEDED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
                HANDLE evt{CreateEvent(nullptr, FALSE, FALSE, nullptr)};
                assert(evt != nullptr, "event");
                assert(SUCCEEDED(q->Signal(fence.Get(), 1)), "signal");
                if (fence->GetCompletedValue() < 1) {
                    fence->SetEventOnCompletion(1, evt);
                    WaitForSingleObject(evt, INFINITE);
                }
                CloseHandle(evt);
            }
        }

        U32 handle{static_cast<U32>(m_VertexBuffers.size())};
        m_VertexBuffers.push_back(std::move(buffer));
        return handle;
    }

    U32 CreateIndexBuffer(const void* data, U64 size) override {
        BufferDesc desc{};
        desc.size = size;
        desc.usage = ResourceUsage::IndexBuffer | ResourceUsage::CopyDest;
        desc.cpuAccessible = false;
        auto buffer{std::make_unique<Buffer>(m_Renderer->GetDevice(), desc)};

        if (data && size) {
            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = size;
            rd.Height = 1;
            rd.DepthOrArraySize = 1;
            rd.MipLevels = 1;
            rd.Format = DXGI_FORMAT_UNKNOWN;
            rd.SampleDesc = {1, 0};
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            rd.Flags = D3D12_RESOURCE_FLAG_NONE;

            ComPtr<ID3D12Resource> upload{};
            assert(SUCCEEDED(m_Renderer->GetDevice().GetDevice()->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))), "Failed to create upload buffer");

            void* mapped{};
            D3D12_RANGE range{0, 0};
            assert(SUCCEEDED(upload->Map(0, &range, &mapped)), "Map failed");
            std::memcpy(mapped, data, static_cast<size_t>(size));
            upload->Unmap(0, nullptr);

            auto& curCL{m_Renderer->GetCurrentCommandList()};
            if (curCL.IsRecording()) {
                auto* cmd{curCL.GetCommandList()};
                D3D12_RESOURCE_BARRIER b0{};
                b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b0.Transition.pResource = buffer->GetResource();
                b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                cmd->ResourceBarrier(1, &b0);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

                cmd->CopyBufferRegion(buffer->GetResource(), 0, upload.Get(), 0, size);

                D3D12_RESOURCE_BARRIER b1{};
                b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b1.Transition.pResource = buffer->GetResource();
                b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                b1.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
                cmd->ResourceBarrier(1, &b1);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_INDEX_BUFFER);

                curCL.KeepAlive(upload);
            } else {
                ComPtr<ID3D12CommandAllocator> alloc{};
                ComPtr<ID3D12GraphicsCommandList> cmd{};
                auto* dev{m_Renderer->GetDevice().GetDevice()};
                auto* q{m_Renderer->GetDevice().GetDirectQueue()};
                assert(SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))), "alloc");
                assert(SUCCEEDED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd))), "cmdlist");

                D3D12_RESOURCE_BARRIER b0{};
                b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b0.Transition.pResource = buffer->GetResource();
                b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                cmd->ResourceBarrier(1, &b0);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

                cmd->CopyBufferRegion(buffer->GetResource(), 0, upload.Get(), 0, size);

                D3D12_RESOURCE_BARRIER b1{};
                b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b1.Transition.pResource = buffer->GetResource();
                b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                b1.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
                cmd->ResourceBarrier(1, &b1);
                buffer->SetCurrentState(D3D12_RESOURCE_STATE_INDEX_BUFFER);

                assert(SUCCEEDED(cmd->Close()), "close");
                ID3D12CommandList* lists[]{cmd.Get()};
                q->ExecuteCommandLists(1, lists);

                ComPtr<ID3D12Fence> fence{};
                assert(SUCCEEDED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
                HANDLE evt{CreateEvent(nullptr, FALSE, FALSE, nullptr)};
                assert(evt != nullptr, "event");
                assert(SUCCEEDED(q->Signal(fence.Get(), 1)), "signal");
                if (fence->GetCompletedValue() < 1) {
                    fence->SetEventOnCompletion(1, evt);
                    WaitForSingleObject(evt, INFINITE);
                }
                CloseHandle(evt);
            }
        }

        U32 handle{static_cast<U32>(m_IndexBuffers.size())};
        m_IndexBuffers.push_back(std::move(buffer));
        return handle;
    }

    void UpdateVertexBuffer(U32 buffer, const void* data, U64 size, U64 dstOffset) override {
        assert(buffer < m_VertexBuffers.size(), "invalid vertex buffer handle");
        assert(data && size > 0, "invalid data");
        UpdateBuffer(*m_VertexBuffers[buffer], data, size, dstOffset);
    }

    void UpdateIndexBuffer(U32 buffer, const void* data, U64 size, U64 dstOffset) override {
        assert(buffer < m_IndexBuffers.size(), "invalid index buffer handle");
        assert(data && size > 0, "invalid data");
        UpdateBuffer(*m_IndexBuffers[buffer], data, size, dstOffset);
    }

    U32 CreateConstantBuffer(U64 size) override {
        U64 alignedSize{(size + 255) & ~255};
        BufferDesc desc{};
        desc.size = alignedSize;
        desc.usage = ResourceUsage::ConstantBuffer;
        desc.cpuAccessible = true;
        auto buffer{std::make_unique<Buffer>(m_Renderer->GetDevice(), desc)};
        U32 handle{static_cast<U32>(m_ConstantBuffers.size())};
        m_ConstantBuffers.push_back(std::move(buffer));
        return handle;
    }

    void UpdateConstantBuffer(U32 buffer, const void *data, U64 size) override {
        if (buffer >= m_ConstantBuffers.size()) return;
        m_ConstantBuffers[buffer]->UpdateData(data, size);
    }

    U32 CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &info) override {
        std::vector<D3D12_ROOT_PARAMETER> rootParams{};
        D3D12_ROOT_PARAMETER p0{};
        p0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p0.Descriptor = {0, 0};
        p0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams.push_back(p0);
        D3D12_ROOT_PARAMETER p1{};
        p1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p1.Descriptor = {1, 0};
        p1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams.push_back(p1);

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = 0;
        D3D12_ROOT_DESCRIPTOR_TABLE table{};
        table.NumDescriptorRanges = 1;
        table.pDescriptorRanges = &range;
        D3D12_ROOT_PARAMETER p2{};
        p2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p2.DescriptorTable = table;
        p2.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams.push_back(p2);

        D3D12_ROOT_PARAMETER p3{};
        p3.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p3.Descriptor = {2, 0};
        p3.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams.push_back(p3);

        D3D12_STATIC_SAMPLER_DESC samp{};
        samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samp.MipLODBias = 0.0f;
        samp.MaxAnisotropy = 1;
        samp.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samp.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        samp.MinLOD = 0.0f;
        samp.MaxLOD = 0.0f;
        samp.ShaderRegister = 0;
        samp.RegisterSpace = 0;
        samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rs{};
        rs.NumParameters = static_cast<UINT>(rootParams.size());
        rs.pParameters = rootParams.data();
        rs.NumStaticSamplers = 1;
        rs.pStaticSamplers = &samp;
        rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        auto rootSig{std::make_unique<RootSignature>(m_Renderer->GetDevice(), rs)};

        GraphicsPipelineDesc pd{};
        for (auto const &sh: info.shaders) {
            std::string target{};
            if (sh.stage == ShaderStage::Vertex) target = "vs_5_1";
            else if (sh.stage == ShaderStage::Pixel) target = "ps_5_1";
            else assert(false, "Unsupported stage");

            ShaderBytecode bc{};
            if (sh.source.find('\n') == std::string::npos && sh.source.ends_with(".hlsl"))
                bc = m_ShaderManager->LoadShader(sh.source, sh.entryPoint, target);
            else bc = m_ShaderManager->CompileShaderFromSource(sh.source, "inline_shader", sh.entryPoint, target);

            if (sh.stage == ShaderStage::Vertex) pd.vertexShader = bc;
            if (sh.stage == ShaderStage::Pixel) pd.pixelShader = bc;
        }

        std::vector<D3D12_INPUT_ELEMENT_DESC> layout{};
        for (auto const &[name, off]: info.vertexAttributes) {
            DXGI_FORMAT fmt{DXGI_FORMAT_R32G32B32_FLOAT};
            if (name == "NORMAL") fmt = DXGI_FORMAT_R32G32B32_FLOAT;
            else if (name == "TEXCOORD") fmt = DXGI_FORMAT_R32G32_FLOAT;
            else if (name == "COLOR") fmt = DXGI_FORMAT_R32_UINT;
            layout.push_back(D3D12_INPUT_ELEMENT_DESC{
                name.c_str(), 0, fmt, 0, off, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            });
        }
        pd.inputLayout = std::move(layout);
        pd.rootSignature = rootSig->GetRootSignature();
        pd.primitiveTopology = ConvertTopology(info.topology);
        pd.numRenderTargets = 1;
        pd.rtvFormats[0] = m_Renderer->GetSwapChain().GetFormat();
        pd.dsvFormat = DXGI_FORMAT_D32_FLOAT;
        if (!info.depthTest) pd.depthStencilState.DepthEnable = FALSE;
        if (!info.depthWrite) pd.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        pd.blendState.RenderTarget[0].BlendEnable = TRUE;
        pd.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pd.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pd.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pd.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pd.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        pd.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pd.blendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        auto pipeline{std::make_unique<GraphicsPipeline>(m_Renderer->GetDevice(), pd)};
        U32 id{static_cast<U32>(m_Pipelines.size())};
        m_Pipelines.push_back(std::move(pipeline));
        m_RootSignatures.push_back(std::move(rootSig));
        m_PipelineTopologies.push_back(ConvertIATopology(info.topology));
        m_PipelineVertexStrides.push_back(info.vertexStride);
        return id;
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
        auto passData{std::make_shared<FramePassData>(std::move(*m_CurrentPassData))};
        m_CurrentPassData = std::make_unique<FramePassData>();
        struct _NoPassData {};
        m_Renderer->GetRenderGraph().AddPass<_NoPassData>(
            passData->passInfo.name,
            [](RenderGraphBuilder &, _NoPassData &) {},
            [this, passData](const RenderGraphResources &, CommandList &cmdList, const _NoPassData &) {
                ExecuteRenderPass(cmdList, *passData);
            }
        );
    }

    void SetPipeline(U32 pipeline) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (pipeline >= m_Pipelines.size()) return;
        m_CurrentPipelineIndex = pipeline;
        const U32 p = pipeline;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            m_Pipelines[p]->Bind(cmd);
            cmd->IASetPrimitiveTopology(m_PipelineTopologies[p]);
        });
    }

    void SetVertexBuffer(U32 buffer) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (buffer >= m_VertexBuffers.size()) return;
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) m_Pipelines[p]->Bind(cmd);
            D3D12_VERTEX_BUFFER_VIEW vbView{};
            vbView.BufferLocation = m_VertexBuffers[buffer]->GetGPUAddress();
            vbView.SizeInBytes = static_cast<UINT>(m_VertexBuffers[buffer]->GetDesc().size);
            vbView.StrideInBytes = (p != UINT_MAX ? m_PipelineVertexStrides[p] : static_cast<UINT>(sizeof(Vertex)));
            cmd->IASetVertexBuffers(0, 1, &vbView);
        });
    }

    void SetIndexBuffer(U32 buffer) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (buffer >= m_IndexBuffers.size()) return;
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) m_Pipelines[p]->Bind(cmd);
            D3D12_INDEX_BUFFER_VIEW ibView{};
            ibView.BufferLocation = m_IndexBuffers[buffer]->GetGPUAddress();
            ibView.SizeInBytes = static_cast<UINT>(m_IndexBuffers[buffer]->GetDesc().size);
            ibView.Format = DXGI_FORMAT_R32_UINT;
            cmd->IASetIndexBuffer(&ibView);
        });
    }

    void Draw(U32 vertexCount, U32 instanceCount, U32 firstVertex, U32 firstInstance) override {
        assert(m_InRenderPass, "Must be in render pass");
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) {
                m_Pipelines[p]->Bind(cmd);
                cmd->IASetPrimitiveTopology(m_PipelineTopologies[p]);
            }
            cmd->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        });
    }

    void DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, S32 vertexOffset, U32 firstInstance) override {
        assert(m_InRenderPass, "Must be in render pass");
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) {
                m_Pipelines[p]->Bind(cmd);
                cmd->IASetPrimitiveTopology(m_PipelineTopologies[p]);
            }
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
        auto tex{std::make_unique<Texture>(m_Renderer->GetDevice(), desc)};
        ID3D12Resource *resource{tex->GetResource()};
        D3D12_RESOURCE_DESC rd{resource->GetDesc()};
        U64 uploadSize{0};
        m_Renderer->GetDevice().GetDevice()->
                GetCopyableFootprints(&rd, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

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
        ComPtr<ID3D12Resource> upload{};
        assert(SUCCEEDED(
            m_Renderer->GetDevice().GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))), "Failed to create upload buffer");

        D3D12_SUBRESOURCE_DATA sub{};
        sub.pData = rgba8;
        sub.RowPitch = static_cast<LONG_PTR>(width) * 4;
        sub.SlicePitch = sub.RowPitch * height;

        auto &curCL{m_Renderer->GetCurrentCommandList()};
        if (curCL.IsRecording()) {
            auto *cmd{curCL.GetCommandList()};
            D3D12_RESOURCE_BARRIER b0{};
            b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b0.Transition.pResource = resource;
            b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            cmd->ResourceBarrier(1, &b0);
            UpdateSubresources(cmd, resource, upload.Get(), 0, 0, 1, &sub);
            curCL.KeepAlive(upload);
            D3D12_RESOURCE_BARRIER b1{};
            b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b1.Transition.pResource = resource;
            b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b1.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmd->ResourceBarrier(1, &b1);
        } else {
            ComPtr<ID3D12CommandAllocator> alloc{};
            ComPtr<ID3D12GraphicsCommandList> cmd{};
            auto *dev{m_Renderer->GetDevice().GetDevice()};
            auto *q{m_Renderer->GetDevice().GetDirectQueue()};
            assert(
                SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))), "alloc");
            assert(SUCCEEDED(
                dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)
                )), "cmdlist");
            D3D12_RESOURCE_BARRIER b0{};
            b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b0.Transition.pResource = resource;
            b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b0.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            cmd->ResourceBarrier(1, &b0);
            UpdateSubresources(cmd.Get(), resource, upload.Get(), 0, 0, 1, &sub);
            D3D12_RESOURCE_BARRIER b1{};
            b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b1.Transition.pResource = resource;
            b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b1.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmd->ResourceBarrier(1, &b1);
            assert(SUCCEEDED(cmd->Close()), "close");
            ID3D12CommandList *lists[]{cmd.Get()};
            q->ExecuteCommandLists(1, lists);
            ComPtr<ID3D12Fence> fence{};
            assert(SUCCEEDED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
            HANDLE evt{CreateEvent(nullptr, FALSE, FALSE, nullptr)};
            assert(evt != nullptr, "event");
            assert(SUCCEEDED(q->Signal(fence.Get(), 1)), "signal");
            if (fence->GetCompletedValue() < 1) {
                fence->SetEventOnCompletion(1, evt);
                WaitForSingleObject(evt, INFINITE);
            }
            CloseHandle(evt);
        }

        auto handle{m_Renderer->GetCbvSrvUavHeap()->Allocate()};
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
        U32 id{static_cast<U32>(m_Textures.size())};
        m_Textures.push_back(std::move(entry));
        return id;
    }

    void SetTexture(U32 texture, U32) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (texture >= m_Textures.size()) return;
        auto gpu{m_Textures[texture].srv.gpuHandle};
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) m_Pipelines[p]->Bind(cmd);
            cmd->SetGraphicsRootDescriptorTable(2, gpu);
        });
    }

    void SetConstantBuffer(U32 buffer, U32 slot) override {
        assert(m_InRenderPass, "Must be in render pass");
        if (buffer >= m_ConstantBuffers.size()) return;
        auto addr{m_ConstantBuffers[buffer]->GetGPUAddress()};
        const U32 p = m_CurrentPipelineIndex;
        std::lock_guard<std::mutex> lk{m_CmdMutex};
        m_CurrentPassData->commands.push_back([=, this](ID3D12GraphicsCommandList *cmd) {
            if (p != UINT_MAX) m_Pipelines[p]->Bind(cmd);
            if (slot == 0) cmd->SetGraphicsRootConstantBufferView(0, addr);
            else if (slot == 1) cmd->SetGraphicsRootConstantBufferView(1, addr);
            else if (slot == 2) cmd->SetGraphicsRootConstantBufferView(3, addr);
        });
    }

private:
    void UpdateBuffer(Buffer& dst, const void* data, U64 size, U64 dstOffset) {
        if (!data || size == 0) { return; }
        assert(dstOffset + size <= dst.GetDesc().size, "update exceeds buffer size");

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = size;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc = {1, 0};
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> upload{};
        assert(SUCCEEDED(m_Renderer->GetDevice().GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))), "upload buffer");

        void* mapped{};
        D3D12_RANGE range{0, 0};
        assert(SUCCEEDED(upload->Map(0, &range, &mapped)), "map");
        std::memcpy(mapped, data, static_cast<size_t>(size));
        upload->Unmap(0, nullptr);

        auto& curCL{m_Renderer->GetCurrentCommandList()};
        auto const u{dst.GetDesc().usage};
        auto FinalState = D3D12_RESOURCE_STATE_GENERIC_READ;
        if (static_cast<U32>(u & ResourceUsage::VertexBuffer)) FinalState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        else if (static_cast<U32>(u & ResourceUsage::IndexBuffer)) FinalState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        else if (static_cast<U32>(u & ResourceUsage::ConstantBuffer)) FinalState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

        if (curCL.IsRecording()) {
            auto* cmd{curCL.GetCommandList()};

            D3D12_RESOURCE_BARRIER b0{};
            b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b0.Transition.pResource = dst.GetResource();
            b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b0.Transition.StateBefore = dst.GetCurrentState();
            b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            if (b0.Transition.StateBefore != b0.Transition.StateAfter) { cmd->ResourceBarrier(1, &b0); }
            dst.SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

            cmd->CopyBufferRegion(dst.GetResource(), dstOffset, upload.Get(), 0, size);

            D3D12_RESOURCE_BARRIER b1{};
            b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b1.Transition.pResource = dst.GetResource();
            b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b1.Transition.StateAfter = FinalState;
            cmd->ResourceBarrier(1, &b1);
            dst.SetCurrentState(FinalState);

            curCL.KeepAlive(upload);
        } else {
            ComPtr<ID3D12CommandAllocator> alloc{};
            ComPtr<ID3D12GraphicsCommandList> cmd{};
            auto* dev{m_Renderer->GetDevice().GetDevice()};
            auto* q{m_Renderer->GetDevice().GetDirectQueue()};
            assert(SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))), "alloc");
            assert(SUCCEEDED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd))), "cmdlist");

            D3D12_RESOURCE_BARRIER b0{};
            b0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b0.Transition.pResource = dst.GetResource();
            b0.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b0.Transition.StateBefore = dst.GetCurrentState();
            b0.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            if (b0.Transition.StateBefore != b0.Transition.StateAfter) { cmd->ResourceBarrier(1, &b0); }
            dst.SetCurrentState(D3D12_RESOURCE_STATE_COPY_DEST);

            cmd->CopyBufferRegion(dst.GetResource(), dstOffset, upload.Get(), 0, size);

            D3D12_RESOURCE_BARRIER b1{};
            b1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b1.Transition.pResource = dst.GetResource();
            b1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b1.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b1.Transition.StateAfter = FinalState;
            cmd->ResourceBarrier(1, &b1);
            dst.SetCurrentState(FinalState);

            assert(SUCCEEDED(cmd->Close()), "close");
            ID3D12CommandList* lists[]{cmd.Get()};
            q->ExecuteCommandLists(1, lists);

            ComPtr<ID3D12Fence> fence{};
            assert(SUCCEEDED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "fence");
            HANDLE evt{CreateEvent(nullptr, FALSE, FALSE, nullptr)};
            assert(evt != nullptr, "event");
            assert(SUCCEEDED(q->Signal(fence.Get(), 1)), "signal");
            if (fence->GetCompletedValue() < 1) {
                fence->SetEventOnCompletion(1, evt);
                WaitForSingleObject(evt, INFINITE);
            }
            CloseHandle(evt);
        }
    }

    void ExecuteRenderPass(CommandList &cmdList, const FramePassData &passData) const {
        auto *cmd{cmdList.GetCommandList()};
        D3D12_VIEWPORT viewport{
            0.0f, 0.0f, static_cast<float>(m_Renderer->GetSwapChain().GetWidth()),
            static_cast<float>(m_Renderer->GetSwapChain().GetHeight()), 0.0f, 1.0f
        };
        D3D12_RECT scissor{
            0, 0, static_cast<LONG>(m_Renderer->GetSwapChain().GetWidth()),
            static_cast<LONG>(m_Renderer->GetSwapChain().GetHeight())
        };
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);
        ID3D12DescriptorHeap *heaps[]{m_Renderer->GetCbvSrvUavHeap()->GetCurrentHeap()};
        cmd->SetDescriptorHeaps(1, heaps);

        Resource rtResource{
            m_Renderer->GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, ResourceType::Texture2D
        };
        rtResource.SetTracked(true);
        cmdList.TransitionResource(rtResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto rtvHandle{m_Renderer->GetCurrentRTV()};
        auto dsvHandle{m_Renderer->GetDSV()};
        cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        if (passData.passInfo.clearColor) {
            cmd->ClearRenderTargetView(rtvHandle, passData.passInfo.clearColorValue, 0, nullptr);
        }
        if (passData.passInfo.clearDepth) {
            cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, passData.passInfo.clearDepthValue, 0, 0,
                                       nullptr);
        }

        for (auto const &command: passData.commands) command(cmd);
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

    static D3D12_PRIMITIVE_TOPOLOGY ConvertIATopology(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            case PrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case PrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case PrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            default: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
};

export std::unique_ptr<IGraphicsContext> CreateDX12GraphicsContext(Window &window, const GraphicsConfig &config) {
    return std::make_unique<DX12GraphicsContext>(window, config);
}
