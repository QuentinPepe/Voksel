module;

#ifdef VOXEL_USE_DX11
#include <d3d11.h>
#include <wrl/client.h>
#endif

export module Graphics.RenderPipeline;

import Core.Types;
import Core.Log;
import Core.Assert;
import Graphics.Mesh;
import Graphics.Shader;
import std;

#ifdef VOXEL_USE_DX11
using Microsoft::WRL::ComPtr;

const char* BASIC_VERTEX_SHADER = R"(
struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(VSInput input){
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
};
)";

const char* BASIC_PIXEL_SHADER = R"(
    struct PSInput {
        float4 position : SV_POSITION;
        float4 color : COLOR;
    };

    float4 main(PSInput input) : SV_TARGET {
        return input.color;
    }
)";

export class RenderPipeline {
private:
    ID3D11Device* m_Device;
    ID3D11DeviceContext* m_Context;

    UniquePtr<ShaderProgram> m_BasicShader;
    ComPtr<ID3D11RasterizerState> m_RasterizeState;
    ComPtr<ID3D11DepthStencilState> m_DepthStencilState;
    ComPtr<ID3D11BlendState> m_BlendState;

public:
    RenderPipeline(ID3D11Device* device, ID3D11DeviceContext* context)
        : m_Device{device}, m_Context{context} {

        CreateShaders();
        CreateRenderStates();
    }

    void BeginFrame() const {
        m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_Context->RSSetState(m_RasterizeState.Get());
        m_Context->OMSetDepthStencilState(m_DepthStencilState.Get(), 0);

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_Context->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xffffffff);
    }

    void DrawMesh(const Mesh& mesh) const {
        m_BasicShader->Bind(m_Context);
        mesh.Bind(m_Context);

        if (mesh.HasIndices()) {
            m_Context->DrawIndexed(mesh.GetIndexCount(), 0, 0);
        } else {
            m_Context->Draw(mesh.GetVertexCount(), 0);
        }
    }

private:
    void CreateShaders() {
        m_BasicShader = std::make_unique<ShaderProgram>();

        ShaderDesc vsDesc{
            .type = ShaderType::Vertex,
            .source = BASIC_VERTEX_SHADER,
            .entrypoint = "main",
            .profile = "vs_5_0"
        };
        auto vertexShader = std::make_unique<Shader>(m_Device, vsDesc);

        ShaderDesc psDesc{
            .type = ShaderType::Pixel,
            .source = BASIC_PIXEL_SHADER,
            .entrypoint = "main",
            .profile = "ps_5_0"
        };
        auto pixelShader = std::make_unique<Shader>(m_Device, psDesc);

        D3D11_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        m_BasicShader->SetVertexShader(std::move(vertexShader));
        m_BasicShader->SetPixelShader(std::move(pixelShader));
        m_BasicShader->CreateInputLayout(m_Device, inputLayout, 2);
    }

    void CreateRenderStates() {
        D3D11_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = 0;
        rasterizerDesc.SlopeScaledDepthBias = 0.0f;
        rasterizerDesc.DepthBiasClamp = 0.0f;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.ScissorEnable = FALSE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;

        HRESULT hr = m_Device->CreateRasterizerState(&rasterizerDesc, &m_RasterizeState);
        assert(SUCCEEDED(hr), "Failed to create rasterize state");

        D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
        depthStencilDesc.StencilEnable = FALSE;

        hr = m_Device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStencilState);
        assert(SUCCEEDED(hr), "Failed to create stencil state");

        D3D11_BLEND_DESC blendDesc{};
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_Device->CreateBlendState(&blendDesc, &m_BlendState);
        assert(SUCCEEDED(hr), "Failed to create blend state");
    }
};

#endif