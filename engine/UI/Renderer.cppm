export module UI.Renderer;

import UI.Core;
import UI.Element;
import UI.Widgets;
import Graphics;
import Core.Types;
import Math.Vector;
import Math.Matrix;
import std;

export struct UIVertex {
    Math::Vec2 position;
    Math::Vec2 uv;
    U32 color;
};

export class UIRenderer {
private:
    IGraphicsContext* m_Graphics{nullptr};
    U32 m_Pipeline{INVALID_INDEX};
    U32 m_VertexBuffer{INVALID_INDEX};
    U32 m_IndexBuffer{INVALID_INDEX};
    U32 m_ConstantBuffer{INVALID_INDEX};

    Vector<UIVertex> m_Vertices;
    Vector<U32> m_Indices;

    Math::Vec2 m_ScreenSize{1280.0f, 720.0f};

public:
    explicit UIRenderer(IGraphicsContext* graphics) : m_Graphics{graphics} {
        Initialize();
    }

    void SetScreenSize(F32 width, F32 height) {
        m_ScreenSize = {width, height};
    }

    void BeginFrame() {
        m_Vertices.clear();
        m_Indices.clear();
    }

    void EndFrame() {
        if (m_Vertices.empty()) return;

        UpdateBuffers();

        struct UIConstants {
            Math::Mat4 projection;
        } constants;

        constants.projection = Math::Mat4::Orthographic(0, m_ScreenSize.x, m_ScreenSize.y, 0, -1, 1);
        m_Graphics->UpdateConstantBuffer(m_ConstantBuffer, &constants, sizeof(constants));

        m_Graphics->SetPipeline(m_Pipeline);
        m_Graphics->SetVertexBuffer(m_VertexBuffer);
        m_Graphics->SetIndexBuffer(m_IndexBuffer);
        m_Graphics->SetConstantBuffer(m_ConstantBuffer, 0);
        m_Graphics->DrawIndexed(static_cast<U32>(m_Indices.size()));
    }

    void DrawRect(const Rect& rect, const Color& color) {
        U32 baseVertex = static_cast<U32>(m_Vertices.size());
        U32 col = color.ToRGBA();

        m_Vertices.push_back({{rect.Left(), rect.Top()}, {0, 0}, col});
        m_Vertices.push_back({{rect.Right(), rect.Top()}, {1, 0}, col});
        m_Vertices.push_back({{rect.Right(), rect.Bottom()}, {1, 1}, col});
        m_Vertices.push_back({{rect.Left(), rect.Bottom()}, {0, 1}, col});

        m_Indices.push_back(baseVertex + 0);
        m_Indices.push_back(baseVertex + 1);
        m_Indices.push_back(baseVertex + 2);
        m_Indices.push_back(baseVertex + 0);
        m_Indices.push_back(baseVertex + 2);
        m_Indices.push_back(baseVertex + 3);
    }

    void DrawRectOutline(const Rect& rect, const Color& color, F32 thickness) {
        DrawRect({rect.Left(), rect.Top(), rect.size.x, thickness}, color);
        DrawRect({rect.Left(), rect.Bottom() - thickness, rect.size.x, thickness}, color);
        DrawRect({rect.Left(), rect.Top() + thickness, thickness, rect.size.y - 2 * thickness}, color);
        DrawRect({rect.Right() - thickness, rect.Top() + thickness, thickness, rect.size.y - 2 * thickness}, color);
    }

    void DrawText(const std::string& text, const Math::Vec2& position, U32 fontSize, const Color& color) {
    }

private:
    void Initialize() {
        CreatePipeline();
        CreateBuffers();
    }

    void CreatePipeline() {
        GraphicsPipelineCreateInfo info{};

        ShaderCode vs{};
        vs.source = R"(
            cbuffer UIConstants : register(b0) {
                float4x4 projection;
            }
            struct VSInput {
                float2 position : POSITION;
                float2 uv : TEXCOORD;
                uint color : COLOR;
            };
            struct VSOutput {
                float4 position : SV_Position;
                float2 uv : TEXCOORD;
                float4 color : COLOR;
            };
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                output.position = mul(float4(input.position, 0, 1), projection);
                output.uv = input.uv;
                uint c = input.color;
                output.color = float4(
                    ((c >> 24) & 0xFF) / 255.0,
                    ((c >> 16) & 0xFF) / 255.0,
                    ((c >> 8) & 0xFF) / 255.0,
                    (c & 0xFF) / 255.0
                );
                return output;
            }
        )";
        vs.entryPoint = "VSMain";
        vs.stage = ShaderStage::Vertex;

        ShaderCode ps{};
        ps.source = R"(
            struct PSInput {
                float4 position : SV_Position;
                float2 uv : TEXCOORD;
                float4 color : COLOR;
            };
            float4 PSMain(PSInput input) : SV_Target {
                return input.color;
            }
        )";
        ps.entryPoint = "PSMain";
        ps.stage = ShaderStage::Pixel;

        info.shaders = {vs, ps};
        info.vertexAttributes = {
            {"POSITION", 0},
            {"TEXCOORD", 8},
            {"COLOR", 16}
        };
        info.vertexStride = sizeof(UIVertex);
        info.topology = PrimitiveTopology::TriangleList;
        info.depthTest = false;
        info.depthWrite = false;

        m_Pipeline = m_Graphics->CreateGraphicsPipeline(info);
        m_ConstantBuffer = m_Graphics->CreateConstantBuffer(sizeof(Math::Mat4));
    }

    void CreateBuffers() {
        const U32 maxVertices = 10000;
        const U32 maxIndices = 30000;

        m_VertexBuffer = m_Graphics->CreateVertexBuffer(nullptr, maxVertices * sizeof(UIVertex));
        m_IndexBuffer = m_Graphics->CreateIndexBuffer(nullptr, maxIndices * sizeof(U32));
    }

    void UpdateBuffers() {
        if (!m_Vertices.empty()) {
            m_VertexBuffer = m_Graphics->CreateVertexBuffer(
                m_Vertices.data(),
                m_Vertices.size() * sizeof(UIVertex)
            );
        }

        if (!m_Indices.empty()) {
            m_IndexBuffer = m_Graphics->CreateIndexBuffer(
                m_Indices.data(),
                m_Indices.size() * sizeof(U32)
            );
        }
    }
};

void UIPanel::OnDraw(UIRenderer* renderer) {
    if (m_BorderWidth > 0) {
        renderer->DrawRectOutline(m_WorldRect, m_BorderColor, m_BorderWidth);
    }
    renderer->DrawRect(m_WorldRect, m_BackgroundColor);
}

void UIText::OnDraw(UIRenderer* renderer) {
    Math::Vec2 textPos = m_WorldRect.Center();

    switch (m_HorizontalAlign) {
        case Alignment::Start:
            textPos.x = m_WorldRect.Left();
            break;
        case Alignment::End:
            textPos.x = m_WorldRect.Right();
            break;
        default:
            break;
    }

    switch (m_VerticalAlign) {
        case Alignment::Start:
            textPos.y = m_WorldRect.Top();
            break;
        case Alignment::End:
            textPos.y = m_WorldRect.Bottom();
            break;
        default:
            break;
    }

    renderer->DrawText(m_Text, textPos, m_FontSize, m_TextColor);
}
