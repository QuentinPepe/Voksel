module;

#ifdef VOXEL_USE_DX11
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#endif

export module Graphics.Shader;

import Core.Types;
import Core.Log;
import Core.Assert;
import std;

using Microsoft::WRL::ComPtr;

#ifdef VOXEL_USE_DX11
using Microsoft::WRL::ComPtr;
#endif

export enum class ShaderType : U8 {
    Vertex,
    Pixel,
    Geometry,
    Compute
};

export struct ShaderDesc {
    ShaderType type;
    std::string source;
    std::string entrypoint = "main";
    std::string profile;
};

#ifdef VOXEL_USE_DX11

export class Shader {
private:
    ShaderType m_Type;
    ComPtr<ID3DBlob> m_Blob;

    std::variant<
        ComPtr<ID3D11VertexShader>,
        ComPtr<ID3D11PixelShader>,
        ComPtr<ID3D11GeometryShader>,
        ComPtr<ID3D11ComputeShader>
    > m_Shader;

public:
    Shader(ID3D11Device* device, const ShaderDesc& desc) : m_Type{desc.type} {
        CompileShader(desc);
        CreateShader(device);
    }

    ~Shader() = default;

    void Bind(ID3D11DeviceContext* context) const {
        switch (m_Type) {
            case ShaderType::Vertex:
                if (auto* vs = std::get_if<ComPtr<ID3D11VertexShader>>(&m_Shader)) {
                    context->VSSetShader(vs->Get(), nullptr, 0);
                }
                break;
            case ShaderType::Pixel:
                if (auto* ps = std::get_if<ComPtr<ID3D11PixelShader>>(&m_Shader)) {
                    context->PSSetShader(ps->Get(), nullptr, 0);
                }
                break;
            case ShaderType::Geometry:
                if (auto* gs = std::get_if<ComPtr<ID3D11GeometryShader>>(&m_Shader)) {
                    context->GSSetShader(gs->Get(), nullptr, 0);
                }
                break;
            case ShaderType::Compute:
                if (auto* cs = std::get_if<ComPtr<ID3D11ComputeShader>>(&m_Shader)) {
                    context->CSSetShader(cs->Get(), nullptr, 0);
                }
                break;
        }
    }

    [[nodiscard]] ID3DBlob* GetBlob() const { return m_Blob.Get(); }
    [[nodiscard]] ShaderType GetType() const { return m_Type; }

private:
    void CompileShader(const ShaderDesc& desc) {
        ComPtr<ID3DBlob> errorBlob;

        UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        HRESULT hr = D3DCompile(
            desc.source.c_str(),
            desc.source.size(),
            nullptr,
            nullptr,
            nullptr,
            desc.entrypoint.c_str(),
            desc.profile.c_str(),
            compileFlags,
            0,
            &m_Blob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                Logger::Error(
                    "Shader compilation failed: {}",
                    static_cast<const char*>(errorBlob->GetBufferPointer())
                );
                assert(false, "Shader compilation failed");
            }
        }
    }

    void CreateShader(ID3D11Device *device) {
        HRESULT hr = S_OK;

        switch (m_Type) {
            case ShaderType::Vertex: {
                ComPtr<ID3D11VertexShader> vs;
                hr = device->CreateVertexShader(
                    m_Blob->GetBufferPointer(),
                    m_Blob->GetBufferSize(),
                    nullptr,
                    &vs
                );
                if (SUCCEEDED(hr)) {
                    m_Shader = std::move(vs);
                }
                break;
            }
            case ShaderType::Pixel: {
                ComPtr<ID3D11PixelShader> ps;
                hr = device->CreatePixelShader(
                    m_Blob->GetBufferPointer(),
                    m_Blob->GetBufferSize(),
                    nullptr,
                    &ps
                );
                if (SUCCEEDED(hr)) {
                    m_Shader = std::move(ps);
                }
                break;
            }
            case ShaderType::Geometry: {
                ComPtr<ID3D11GeometryShader> gs;
                hr = device->CreateGeometryShader(
                    m_Blob->GetBufferPointer(),
                    m_Blob->GetBufferSize(),
                    nullptr,
                    &gs
                );
                if (SUCCEEDED(hr)) {
                    m_Shader = std::move(gs);
                }
                break;
            }
            case ShaderType::Compute: {
                ComPtr<ID3D11ComputeShader> cs;
                hr = device->CreateComputeShader(
                    m_Blob->GetBufferPointer(),
                    m_Blob->GetBufferSize(),
                    nullptr,
                    &cs
                );
                if (SUCCEEDED(hr)) {
                    m_Shader = std::move(cs);
                }
                break;
            }
        }
        assert(SUCCEEDED(hr), "Failed to create shader");
    }
};

export class ShaderProgram {
private:
    UniquePtr<Shader> m_VertexShader;
    UniquePtr<Shader> m_PixelShader;
    UniquePtr<Shader> m_GeometryShader;
    ComPtr<ID3D11InputLayout> m_InputLayout;

public:
    ShaderProgram() = default;

    void SetVertexShader(UniquePtr<Shader> shader) {
        assert(shader->GetType() == ShaderType::Vertex, "Not a vertex shader");
        m_VertexShader = std::move(shader);
    }

    void SetPixelShader(UniquePtr<Shader> shader) {
        assert(shader->GetType() == ShaderType::Pixel, "Not a pixel shader");
        m_PixelShader = std::move(shader);
    }

    void SetGeometryShader(UniquePtr<Shader> shader) {
        assert(shader->GetType() == ShaderType::Geometry, "Not a vertex geometry");
        m_GeometryShader = std::move(shader);
    }

    void CreateInputLayout(ID3D11Device* device, const D3D11_INPUT_ELEMENT_DESC* elements, UINT numElements) {
        assert(m_VertexShader.get(), "Vertex shader required for input layout");

        HRESULT hr = device->CreateInputLayout(
            elements,
            numElements,
            m_VertexShader->GetBlob()->GetBufferPointer(),
            m_VertexShader->GetBlob()->GetBufferSize(),
            &m_InputLayout
        );

        assert(SUCCEEDED(hr), "Failed to create input layout");
    }

    void Bind(ID3D11DeviceContext* context) const {
        if (m_InputLayout) {
            context->IASetInputLayout(m_InputLayout.Get());
        }
        if (m_VertexShader) {
            m_VertexShader->Bind(context);
        }
        if (m_PixelShader) {
            m_PixelShader->Bind(context);
        }
        if (m_GeometryShader) {
            m_GeometryShader->Bind(context);
        }
    }

    [[nodiscard]] Shader* GetVertexShader() const { return m_VertexShader.get(); }
    [[nodiscard]] Shader* GetPixelShader() const { return m_PixelShader.get(); }
};

#endif