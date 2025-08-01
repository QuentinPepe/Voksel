module;
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.Pipeline;

import Core.Assert;
import Core.Types;
import Core.Log;
import Graphics.DX12.Device;
import std;

using Microsoft::WRL::ComPtr;

export struct ShaderBytecode {
    Vector<U8> data;

    D3D12_SHADER_BYTECODE GetD3D12Bytecode() const {
        return {data.data(), data.size()};
    }
};

export ShaderBytecode CompileShader(const std::string& source, const std::string& entryPoint, const std::string& target) {
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(
        source.c_str(),
        source.size(),
        nullptr,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        target.c_str(),
        compileFlags,
        0,
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Error(LogGraphics, "Shader compilation failed: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        assert(false, "Failed to compile shader");
    }

    ShaderBytecode bytecode;
    bytecode.data.resize(shaderBlob->GetBufferSize());
    std::memcpy(bytecode.data.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

    return bytecode;
}

export struct GraphicsPipelineDesc {
    ShaderBytecode vertexShader;
    ShaderBytecode pixelShader;
    ShaderBytecode geometryShader;
    ShaderBytecode hullShader;
    ShaderBytecode domainShader;

    Vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    D3D12_RASTERIZER_DESC rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    D3D12_BLEND_DESC blendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    D3D12_DEPTH_STENCIL_DESC depthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    U32 sampleMask = UINT_MAX;
    U32 numRenderTargets = 1;
    DXGI_FORMAT rtvFormats[8] = {DXGI_FORMAT_R8G8B8A8_UNORM};
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT;
    DXGI_SAMPLE_DESC sampleDesc = {1, 0};

    ID3D12RootSignature* rootSignature = nullptr;
};

export class GraphicsPipeline {
private:
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    GraphicsPipeline(Device& device, const GraphicsPipelineDesc& desc) {
        assert(desc.rootSignature, "Root signature is required");
        m_RootSignature = desc.rootSignature;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = desc.rootSignature;

        psoDesc.VS = desc.vertexShader.GetD3D12Bytecode();
        psoDesc.PS = desc.pixelShader.GetD3D12Bytecode();

        if (!desc.geometryShader.data.empty()) {
            psoDesc.GS = desc.geometryShader.GetD3D12Bytecode();
        }
        if (!desc.hullShader.data.empty()) {
            psoDesc.HS = desc.hullShader.GetD3D12Bytecode();
        }
        if (!desc.domainShader.data.empty()) {
            psoDesc.DS = desc.domainShader.GetD3D12Bytecode();
        }

        psoDesc.InputLayout = {desc.inputLayout.data(), static_cast<UINT>(desc.inputLayout.size())};
        psoDesc.PrimitiveTopologyType = desc.primitiveTopology;
        psoDesc.RasterizerState = desc.rasterizerState;
        psoDesc.BlendState = desc.blendState;
        psoDesc.DepthStencilState = desc.depthStencilState;
        psoDesc.SampleMask = desc.sampleMask;
        psoDesc.NumRenderTargets = desc.numRenderTargets;

        for (U32 i = 0; i < desc.numRenderTargets; ++i) {
            psoDesc.RTVFormats[i] = desc.rtvFormats[i];
        }

        psoDesc.DSVFormat = desc.dsvFormat;
        psoDesc.SampleDesc = desc.sampleDesc;

        assert(SUCCEEDED(device.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState))),
               "Failed to create graphics pipeline state");
    }

    void Bind(ID3D12GraphicsCommandList* cmdList) const {
        cmdList->SetPipelineState(m_PipelineState.Get());
        cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
    }

    [[nodiscard]] ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }
    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
};

export struct ComputePipelineDesc {
    ShaderBytecode computeShader;
    ID3D12RootSignature* rootSignature = nullptr;
};

export class ComputePipeline {
private:
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    ComputePipeline(Device& device, const ComputePipelineDesc& desc) {
        assert(desc.rootSignature, "Root signature is required");
        m_RootSignature = desc.rootSignature;

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = desc.rootSignature;
        psoDesc.CS = desc.computeShader.GetD3D12Bytecode();

        assert(SUCCEEDED(device.GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState))),
               "Failed to create compute pipeline state");
    }

    void Bind(ID3D12GraphicsCommandList* cmdList) const {
        cmdList->SetPipelineState(m_PipelineState.Get());
        cmdList->SetComputeRootSignature(m_RootSignature.Get());
    }

    [[nodiscard]] ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }
    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
};

export class RootSignature {
private:
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    RootSignature(Device& device, const D3D12_ROOT_SIGNATURE_DESC& desc) {
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error);

        if (FAILED(hr)) {
            if (error) {
                Logger::Error(LogGraphics, "Root signature serialization failed: {}", static_cast<const char*>(error->GetBufferPointer()));
            }
            assert(false, "Failed to serialize root signature");
        }

        assert(SUCCEEDED(device.GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_RootSignature)
        )), "Failed to create root signature");
    }

    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
    [[nodiscard]] operator ID3D12RootSignature*() const { return m_RootSignature.Get(); }
};