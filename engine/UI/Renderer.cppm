export module UI.Renderer;

import UI.Core;
import UI.Element;
import UI.Widgets;
import UI.Font;
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
    std::shared_ptr<UIFont> m_Font{};
    U32 m_WhiteTex{INVALID_INDEX};
    U32 m_CurrentTexture{INVALID_INDEX};

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
        m_CurrentTexture = INVALID_INDEX;
    }

    void EndFrame() {
        Flush();
    }

    void DrawRect(const Rect& rect, const Color& color) {
        BindTexture(m_WhiteTex);
        U32 base = static_cast<U32>(m_Vertices.size());
        U32 col = color.ToRGBA();
        Math::Vec2 uv{-1.0f, -1.0f};
        m_Vertices.push_back({{rect.Left(),  rect.Top()},    uv, col});
        m_Vertices.push_back({{rect.Right(), rect.Top()},    uv, col});
        m_Vertices.push_back({{rect.Right(), rect.Bottom()}, uv, col});
        m_Vertices.push_back({{rect.Left(),  rect.Bottom()}, uv, col});
        m_Indices.insert(m_Indices.end(), {base+0,base+1,base+2, base+0,base+2,base+3});
    }

    void DrawRectOutline(const Rect& rect, const Color& color, F32 thickness) {
        DrawRect({rect.Left(), rect.Top(), rect.size.x, thickness}, color);
        DrawRect({rect.Left(), rect.Bottom() - thickness, rect.size.x, thickness}, color);
        DrawRect({rect.Left(), rect.Top() + thickness, thickness, rect.size.y - 2 * thickness}, color);
        DrawRect({rect.Right() - thickness, rect.Top() + thickness, thickness, rect.size.y - 2 * thickness}, color);
    }

    void DrawText(const std::string& text, const Rect& r, U32 fontSize, const Color& color,
                  Alignment hAlign, Alignment vAlign) {
        if (!m_Font) return;
        BindTexture(m_Font->GetTexture());
        F32 px{static_cast<F32>(fontSize)};
        auto size{m_Font->MeasureText(text, px)};
        Math::Vec2 pos{r.position};
        if (hAlign == Alignment::Center) pos.x += (r.size.x - size.x) * 0.5f;
        else if (hAlign == Alignment::End) pos.x += (r.size.x - size.x);
        F32 lineH{(m_Font->GetAscent() - m_Font->GetDescent() + m_Font->GetLineGap()) * (px / m_Font->GetBakePx())};
        F32 totalH{size.y};
        if (vAlign == Alignment::Center) pos.y += (r.size.y - totalH) * 0.5f;
        else if (vAlign == Alignment::End) pos.y += (r.size.y - totalH);
        F32 s{px / m_Font->GetBakePx()};
        F32 cursorX{0.0f};
        F32 baselineY{pos.y + m_Font->GetAscent() * s};
        U32 col = color.ToRGBA();
        U32 prev{0};
        for (size_t i{0}; i < text.size();) {
            U32 cp{};
            size_t n{UIFont::DecodeUTF8(text, i, cp)};
            if (n == 0) break;
            i += n;
            if (cp == '\n') {
                cursorX = 0.0f;
                baselineY += lineH;
                prev = 0;
                continue;
            }
            if (prev) cursorX += m_Font->KerningAtBakePx(prev, cp) * s;
            F32 x0{}, y0{}, x1{}, y1{}, u0{}, v0{}, u1{}, v1{}, adv{};
            m_Font->GetQuad(cp, cursorX / s, 0.0f, x0, y0, x1, y1, u0, v0, u1, v1, adv);
            F32 qx0{pos.x + x0 * s};
            F32 qy0{baselineY + y0 * s};
            F32 qx1{pos.x + x1 * s};
            F32 qy1{baselineY + y1 * s};
            U32 base = static_cast<U32>(m_Vertices.size());
            m_Vertices.push_back({{qx0, qy0}, {u0, v0}, col});
            m_Vertices.push_back({{qx1, qy0}, {u1, v0}, col});
            m_Vertices.push_back({{qx1, qy1}, {u1, v1}, col});
            m_Vertices.push_back({{qx0, qy1}, {u0, v1}, col});
            m_Indices.insert(m_Indices.end(), {base+0,base+1,base+2, base+0,base+2,base+3});
            cursorX += adv * s;
            prev = cp;
        }
    }

    void DrawText(const std::string& text, const Math::Vec2& position, U32 fontSize, const Color& color) {
        Rect r{position, {1e6f, 1e6f}};
        DrawText(text, r, fontSize, color, Alignment::Start, Alignment::Start);
    }

    void DrawImage(const Rect& rect, U32 texture, const Math::Vec4& uv01, const Color& tint = Color::White) {
        BindTexture(texture);
        U32 base = static_cast<U32>(m_Vertices.size());
        U32 col = tint.ToRGBA();
        m_Vertices.push_back({{rect.Left(),  rect.Top()},    {uv01.x, uv01.y}, col});
        m_Vertices.push_back({{rect.Right(), rect.Top()},    {uv01.z, uv01.y}, col});
        m_Vertices.push_back({{rect.Right(), rect.Bottom()}, {uv01.z, uv01.w}, col});
        m_Vertices.push_back({{rect.Left(),  rect.Bottom()}, {uv01.x, uv01.w}, col});
        m_Indices.insert(m_Indices.end(), {base+0,base+1,base+2, base+0,base+2,base+3});
    }

    void SetFont(std::shared_ptr<UIFont> font) { m_Font = std::move(font); }
    IGraphicsContext* GetGraphics() { return m_Graphics; }

private:
    void Initialize() {
        CreatePipeline();
        CreateBuffers();
        U8 w[4]{255,255,255,255};
        m_WhiteTex = m_Graphics->CreateTexture2D(w, 1, 1, 1);
    }

    void CreatePipeline() {
        GraphicsPipelineCreateInfo info{};
        ShaderCode vs{};
        vs.source = R"(
            cbuffer UIConstants : register(b0) { float4x4 projection; }
            struct VSInput { float2 position:POSITION; float2 uv:TEXCOORD; uint color:COLOR; };
            struct VSOutput { float4 position:SV_Position; float2 uv:TEXCOORD; float4 color:COLOR; };
            VSOutput VSMain(VSInput i){
                VSOutput o;
                o.position = mul(float4(i.position,0,1), projection);
                o.uv = i.uv;
                uint c = i.color;
                o.color = float4(((c>>24)&255)/255.0, ((c>>16)&255)/255.0, ((c>>8)&255)/255.0, (c&255)/255.0);
                return o;
            }
        )";
        vs.entryPoint = "VSMain";
        vs.stage = ShaderStage::Vertex;
        ShaderCode ps{};
        ps.source = R"(
            Texture2D gTex : register(t0);
            SamplerState gSamp : register(s0);
            struct PSInput { float4 position:SV_Position; float2 uv:TEXCOORD; float4 color:COLOR; };
            float4 PSMain(PSInput i):SV_Target{
                float4 tex = (i.uv.x < 0.0) ? float4(1,1,1,1) : gTex.Sample(gSamp, i.uv);
                return tex * i.color;
            }
        )";
        ps.entryPoint = "PSMain";
        ps.stage = ShaderStage::Pixel;
        info.shaders = {vs, ps};
        info.vertexAttributes = {{"POSITION", 0}, {"TEXCOORD", 8}, {"COLOR", 16}};
        info.vertexStride = sizeof(UIVertex);
        info.topology = PrimitiveTopology::TriangleList;
        info.depthTest = false;
        info.depthWrite = false;
        m_Pipeline = m_Graphics->CreateGraphicsPipeline(info);
        m_ConstantBuffer = m_Graphics->CreateConstantBuffer(sizeof(Math::Mat4));
    }

    void CreateBuffers() {
        const U32 maxVertices{10000};
        const U32 maxIndices{30000};
        m_VertexBuffer = m_Graphics->CreateVertexBuffer(nullptr, maxVertices * sizeof(UIVertex));
        m_IndexBuffer = m_Graphics->CreateIndexBuffer(nullptr, maxIndices * sizeof(U32));
    }

    void UpdateBuffers() {
        if (!m_Vertices.empty()) {
            m_VertexBuffer = m_Graphics->CreateVertexBuffer(m_Vertices.data(), m_Vertices.size() * sizeof(UIVertex));
        }
        if (!m_Indices.empty()) {
            m_IndexBuffer = m_Graphics->CreateIndexBuffer(m_Indices.data(), m_Indices.size() * sizeof(U32));
        }
    }

    void Flush() {
        if (m_Vertices.empty()) return;
        UpdateBuffers();
        struct UIConstants { Math::Mat4 projection; } constants{};
        constants.projection = Math::Mat4::Orthographic(0, m_ScreenSize.x, m_ScreenSize.y, 0, -1, 1);
        m_Graphics->UpdateConstantBuffer(m_ConstantBuffer, &constants, sizeof(constants));
        m_Graphics->SetPipeline(m_Pipeline);
        m_Graphics->SetVertexBuffer(m_VertexBuffer);
        m_Graphics->SetIndexBuffer(m_IndexBuffer);
        m_Graphics->SetConstantBuffer(m_ConstantBuffer, 0);
        if (m_CurrentTexture == INVALID_INDEX) m_CurrentTexture = m_WhiteTex;
        m_Graphics->SetTexture(m_CurrentTexture, 0);
        m_Graphics->DrawIndexed(static_cast<U32>(m_Indices.size()));
        m_Vertices.clear();
        m_Indices.clear();
    }

    void BindTexture(U32 tex) {
        if (tex == INVALID_INDEX) tex = m_WhiteTex;
        if (m_CurrentTexture != tex) {
            Flush();
            m_CurrentTexture = tex;
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
    renderer->DrawText(m_Text, m_WorldRect, m_FontSize, m_TextColor, m_HorizontalAlign, m_VerticalAlign);
}

void UIImage::OnDraw(UIRenderer* renderer) {
    if (m_Texture != INVALID_INDEX) {
        renderer->DrawImage(m_WorldRect, m_Texture, m_UV, m_Tint);
    }
}
