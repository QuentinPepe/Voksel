module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <wrl/client.h>
#include <algorithm>
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

export ShaderBytecode CompileShader(const std::string &source, const std::string &entryPoint,
                                    const std::string &target) {
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
            Logger::Error(LogGraphics, "Shader compilation failed: {}",
                          static_cast<const char *>(errorBlob->GetBufferPointer()));
        }
        assert(false, "Failed to compile shader");
    }
    ShaderBytecode bytecode;
    bytecode.data.resize(shaderBlob->GetBufferSize());
    std::memcpy(bytecode.data.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
    return bytecode;
}

static void ReflectMaxRegisters(const ShaderBytecode& bc, int& cbvMax, int& srvMax, int& uavMax, int& sampMax) {
    if (bc.data.empty()) return;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> r;
    if (FAILED(D3DReflect(bc.data.data(), bc.data.size(), IID_PPV_ARGS(&r)))) return;
    D3D12_SHADER_DESC sd{};
    if (FAILED(r->GetDesc(&sd))) return;
    for (UINT i = 0; i < sd.BoundResources; ++i) {
        D3D12_SHADER_INPUT_BIND_DESC bd{};
        if (FAILED(r->GetResourceBindingDesc(i, &bd))) continue;
        if (bd.Space != 0) continue;
        if (bd.Type == D3D_SIT_CBUFFER) cbvMax = (std::max)(cbvMax, int(bd.BindPoint + bd.BindCount - 1));
        if (bd.Type == D3D_SIT_TBUFFER || bd.Type == D3D_SIT_TEXTURE || bd.Type == D3D_SIT_STRUCTURED || bd.Type == D3D_SIT_BYTEADDRESS)
            srvMax = (std::max)(srvMax, int(bd.BindPoint + bd.BindCount - 1));
        if (bd.Type == D3D_SIT_UAV_RWTYPED || bd.Type == D3D_SIT_UAV_RWSTRUCTURED || bd.Type == D3D_SIT_UAV_RWBYTEADDRESS || bd.Type == D3D_SIT_UAV_APPEND_STRUCTURED || bd.Type == D3D_SIT_UAV_CONSUME_STRUCTURED || bd.Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER)
            uavMax = (std::max)(uavMax, int(bd.BindPoint + bd.BindCount - 1));
        if (bd.Type == D3D_SIT_SAMPLER) sampMax = (std::max)(sampMax, int(bd.BindPoint + bd.BindCount - 1));
    }
}

static ComPtr<ID3D12RootSignature> BuildAutoGraphicsRootSignature(Device& device, const ShaderBytecode& vs, const ShaderBytecode& ps, const ShaderBytecode& gs, const ShaderBytecode& hs, const ShaderBytecode& ds) {
    int cbvMax = -1, srvMax = -1, uavMax = -1, sampMax = -1;
    ReflectMaxRegisters(vs, cbvMax, srvMax, uavMax, sampMax);
    ReflectMaxRegisters(ps, cbvMax, srvMax, uavMax, sampMax);
    ReflectMaxRegisters(gs, cbvMax, srvMax, uavMax, sampMax);
    ReflectMaxRegisters(hs, cbvMax, srvMax, uavMax, sampMax);
    ReflectMaxRegisters(ds, cbvMax, srvMax, uavMax, sampMax);
    if (cbvMax < 0) cbvMax = 0;

    std::vector<CD3DX12_ROOT_PARAMETER> params;
    params.reserve(size_t(cbvMax + 1) + 2);
    for (int i = 0; i <= cbvMax; ++i) {
        CD3DX12_ROOT_PARAMETER p;
        p.InitAsConstantBufferView(UINT(i), 0, D3D12_SHADER_VISIBILITY_ALL);
        params.push_back(p);
    }

    std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    if (srvMax >= 0) {
        CD3DX12_DESCRIPTOR_RANGE r;
        r.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT(srvMax + 1), 0, 0);
        ranges.push_back(r);
    }
    if (uavMax >= 0) {
        CD3DX12_DESCRIPTOR_RANGE r;
        r.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UINT(uavMax + 1), 0, 0);
        ranges.push_back(r);
    }
    if (!ranges.empty()) {
        CD3DX12_ROOT_PARAMETER p;
        p.InitAsDescriptorTable(UINT(ranges.size()), ranges.data(), D3D12_SHADER_VISIBILITY_ALL);
        params.push_back(p);
    }

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
    for (int i = 0; i <= sampMax; ++i) {
        D3D12_STATIC_SAMPLER_DESC s{};
        s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.MipLODBias = 0.0f;
        s.MaxAnisotropy = 1;
        s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        s.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        s.MinLOD = 0.0f;
        s.MaxLOD = D3D12_FLOAT32_MAX;
        s.ShaderRegister = UINT(i);
        s.RegisterSpace = 0;
        s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplers.push_back(s);
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = UINT(params.size());
    desc.pParameters = params.empty() ? nullptr : params.data();
    desc.NumStaticSamplers = UINT(samplers.size());
    desc.pStaticSamplers = samplers.empty() ? nullptr : samplers.data();
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig;
    ComPtr<ID3DBlob> err;
    assert(SUCCEEDED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)), "Failed to serialize root signature");
    ComPtr<ID3D12RootSignature> rs;
    assert(SUCCEEDED(device.GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs))), "Failed to create root signature");
    return rs;
}

static ComPtr<ID3D12RootSignature> BuildAutoComputeRootSignature(Device& device, const ShaderBytecode& cs) {
    int cbvMax = -1, srvMax = -1, uavMax = -1, sampMax = -1;
    ReflectMaxRegisters(cs, cbvMax, srvMax, uavMax, sampMax);
    if (cbvMax < 0) cbvMax = 0;

    std::vector<CD3DX12_ROOT_PARAMETER> params;
    params.reserve(size_t(cbvMax + 1) + 1);
    for (int i = 0; i <= cbvMax; ++i) {
        CD3DX12_ROOT_PARAMETER p;
        p.InitAsConstantBufferView(UINT(i), 0, D3D12_SHADER_VISIBILITY_ALL);
        params.push_back(p);
    }

    std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
    if (srvMax >= 0) {
        CD3DX12_DESCRIPTOR_RANGE r;
        r.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT(srvMax + 1), 0, 0);
        ranges.push_back(r);
    }
    if (uavMax >= 0) {
        CD3DX12_DESCRIPTOR_RANGE r;
        r.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UINT(uavMax + 1), 0, 0);
        ranges.push_back(r);
    }
    if (!ranges.empty()) {
        CD3DX12_ROOT_PARAMETER p;
        p.InitAsDescriptorTable(UINT(ranges.size()), ranges.data(), D3D12_SHADER_VISIBILITY_ALL);
        params.push_back(p);
    }

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
    for (int i = 0; i <= sampMax; ++i) {
        D3D12_STATIC_SAMPLER_DESC s{};
        s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.MipLODBias = 0.0f;
        s.MaxAnisotropy = 1;
        s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        s.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        s.MinLOD = 0.0f;
        s.MaxLOD = D3D12_FLOAT32_MAX;
        s.ShaderRegister = UINT(i);
        s.RegisterSpace = 0;
        s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        samplers.push_back(s);
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = UINT(params.size());
    desc.pParameters = params.empty() ? nullptr : params.data();
    desc.NumStaticSamplers = UINT(samplers.size());
    desc.pStaticSamplers = samplers.empty() ? nullptr : samplers.data();
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sig;
    ComPtr<ID3DBlob> err;
    assert(SUCCEEDED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)), "Failed to serialize compute root signature");
    ComPtr<ID3D12RootSignature> rs;
    assert(SUCCEEDED(device.GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs))), "Failed to create compute root signature");
    return rs;
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
    ID3D12RootSignature *rootSignature = nullptr;
};

export class GraphicsPipeline {
private:
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    GraphicsPipeline(Device &device, const GraphicsPipelineDesc &desc) {
        if (desc.rootSignature) {
            m_RootSignature = desc.rootSignature;
            m_RootSignature->AddRef();
        } else {
            m_RootSignature = BuildAutoGraphicsRootSignature(device, desc.vertexShader, desc.pixelShader, desc.geometryShader, desc.hullShader, desc.domainShader);
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_RootSignature.Get();
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

    void Bind(ID3D12GraphicsCommandList *cmdList) const {
        cmdList->SetPipelineState(m_PipelineState.Get());
        cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
    }

    [[nodiscard]] ID3D12PipelineState *GetPipelineState() const { return m_PipelineState.Get(); }
    [[nodiscard]] ID3D12RootSignature *GetRootSignature() const { return m_RootSignature.Get(); }
};

export struct ComputePipelineDesc {
    ShaderBytecode computeShader;
    ID3D12RootSignature *rootSignature = nullptr;
};

export class ComputePipeline {
private:
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    ComputePipeline(Device &device, const ComputePipelineDesc &desc) {
        if (desc.rootSignature) {
            m_RootSignature = desc.rootSignature;
            m_RootSignature->AddRef();
        } else {
            m_RootSignature = BuildAutoComputeRootSignature(device, desc.computeShader);
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_RootSignature.Get();
        psoDesc.CS = desc.computeShader.GetD3D12Bytecode();
        assert(SUCCEEDED(device.GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState))),
               "Failed to create compute pipeline state");
    }

    void Bind(ID3D12GraphicsCommandList *cmdList) const {
        cmdList->SetPipelineState(m_PipelineState.Get());
        cmdList->SetComputeRootSignature(m_RootSignature.Get());
    }

    [[nodiscard]] ID3D12PipelineState *GetPipelineState() const { return m_PipelineState.Get(); }
    [[nodiscard]] ID3D12RootSignature *GetRootSignature() const { return m_RootSignature.Get(); }
};

export class RootSignature {
private:
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    RootSignature(Device &device, const D3D12_ROOT_SIGNATURE_DESC &desc) {
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                Logger::Error(LogGraphics, "Root signature serialization failed: {}",
                              static_cast<const char *>(error->GetBufferPointer()));
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

    [[nodiscard]] ID3D12RootSignature *GetRootSignature() const { return m_RootSignature.Get(); }
    [[nodiscard]] operator ID3D12RootSignature *() const { return m_RootSignature.Get(); }
};
