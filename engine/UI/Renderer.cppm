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
    static constexpr U32 MAX_VERTICES = 65536;
    static constexpr U32 MAX_INDICES = 98304;
    static constexpr U32 MAX_DRAW_CALLS = 256;
    Vector<UIVertex> m_Vertices;
    Vector<U32> m_Indices;
    struct DrawCall {
        U32 indexCount;
        U32 indexOffset;
        U32 texture;
    };
    Vector<DrawCall> m_DrawCalls;
    Math::Vec2 m_ScreenSize{1280.0f, 720.0f};
    std::shared_ptr<UIFont> m_Font{};
    U32 m_WhiteTex{INVALID_INDEX};
    U32 m_CurrentTexture{INVALID_INDEX};
    struct VertexCache {
        Vector<UIVertex> vertices;
        Vector<U32> indices;
        bool dirty{true};
    };
    UnorderedMap<U32, VertexCache> m_ElementCache;
    U32 m_FrameCounter{0};
    U32 m_BatchStartIndex{0};

public:
    explicit UIRenderer(IGraphicsContext* graphics) : m_Graphics{graphics} {
        Initialize();
        m_Vertices.reserve(MAX_VERTICES);
        m_Indices.reserve(MAX_INDICES);
        m_DrawCalls.reserve(MAX_DRAW_CALLS);
    }

    void SetScreenSize(F32 width, F32 height) {
        m_ScreenSize = {width, height};
    }

    void BeginFrame() {
        m_Vertices.clear();
        m_Indices.clear();
        m_DrawCalls.clear();
        m_CurrentTexture = INVALID_INDEX;
        m_BatchStartIndex = 0;
        m_FrameCounter++;
    }

    void EndFrame() {
        if (m_DrawCalls.empty()) {
            if (!m_Vertices.empty()) {
                m_Graphics->UpdateVertexBuffer(m_VertexBuffer, m_Vertices.data(), m_Vertices.size() * sizeof(UIVertex));
            }
            if (!m_Indices.empty()) {
                m_Graphics->UpdateIndexBuffer(m_IndexBuffer, m_Indices.data(), m_Indices.size() * sizeof(U32));
            }
            struct UIConstants { Math::Mat4 projection; } constants{};
            constants.projection = Math::Mat4::Orthographic(0, m_ScreenSize.x, m_ScreenSize.y, 0, -1, 1);
            m_Graphics->UpdateConstantBuffer(m_ConstantBuffer, &constants, sizeof(constants));
            m_Graphics->SetPipeline(m_Pipeline);
            m_Graphics->SetVertexBuffer(m_VertexBuffer);
            m_Graphics->SetIndexBuffer(m_IndexBuffer);
            m_Graphics->SetConstantBuffer(m_ConstantBuffer, 0);
            FlushBatch();
            if (m_DrawCalls.empty()) return;
        } else {
            if (!m_Vertices.empty()) {
                m_Graphics->UpdateVertexBuffer(m_VertexBuffer, m_Vertices.data(), m_Vertices.size() * sizeof(UIVertex));
            }
            if (!m_Indices.empty()) {
                m_Graphics->UpdateIndexBuffer(m_IndexBuffer, m_Indices.data(), m_Indices.size() * sizeof(U32));
            }
            struct UIConstants { Math::Mat4 projection; } constants{};
            constants.projection = Math::Mat4::Orthographic(0, m_ScreenSize.x, m_ScreenSize.y, 0, -1, 1);
            m_Graphics->UpdateConstantBuffer(m_ConstantBuffer, &constants, sizeof(constants));
            m_Graphics->SetPipeline(m_Pipeline);
            m_Graphics->SetVertexBuffer(m_VertexBuffer);
            m_Graphics->SetIndexBuffer(m_IndexBuffer);
            m_Graphics->SetConstantBuffer(m_ConstantBuffer, 0);
            FlushBatch();
        }
        for (auto const& draw : m_DrawCalls) {
            if (draw.texture != INVALID_INDEX) {
                m_Graphics->SetTexture(draw.texture, 0);
            }
            m_Graphics->DrawIndexed(draw.indexCount, 1, draw.indexOffset, 0, 0);
        }
    }

    void DrawRect(const Rect& rect, const Color& color) {
        AddQuad(rect, color, {-1.0f, -1.0f, -1.0f, -1.0f}, m_WhiteTex);
    }

    void DrawRectOutline(const Rect& rect, const Color& color, F32 thickness) {
        const F32 l{rect.Left()}, t{rect.Top()};
        const F32 r{rect.Right()}, b{rect.Bottom()};
        AddQuad({l, t, rect.size.x, thickness}, color, {-1, -1, -1, -1}, m_WhiteTex);
        AddQuad({l, b - thickness, rect.size.x, thickness}, color, {-1, -1, -1, -1}, m_WhiteTex);
        AddQuad({l, t + thickness, thickness, rect.size.y - 2 * thickness}, color, {-1, -1, -1, -1}, m_WhiteTex);
        AddQuad({r - thickness, t + thickness, thickness, rect.size.y - 2 * thickness}, color, {-1, -1, -1, -1}, m_WhiteTex);
    }

    void DrawText(const std::string& text, const Rect& r, U32 fontSize, const Color& color,
                  Alignment hAlign, Alignment vAlign) {
        if (!m_Font || text.empty()) return;
        U32 textHash{static_cast<U32>(std::hash<std::string>{}(text) ^ fontSize)};
        auto& cache{m_ElementCache[textHash]};
        if (cache.dirty || m_FrameCounter % 60 == 0) {
            cache.vertices.clear();
            cache.indices.clear();
            GenerateTextGeometry(text, fontSize, cache.vertices, cache.indices);
            cache.dirty = false;
        }
        F32 px{static_cast<F32>(fontSize)};
        auto size{m_Font->MeasureText(text, px)};
        Math::Vec2 pos{r.position};
        if (hAlign == Alignment::Center) pos.x += (r.size.x - size.x) * 0.5f;
        else if (hAlign == Alignment::End) pos.x += (r.size.x - size.x);
        F32 lineH{(m_Font->GetAscent() - m_Font->GetDescent() + m_Font->GetLineGap()) * (px / m_Font->GetBakePx())};
        if (vAlign == Alignment::Center) pos.y += (r.size.y - size.y) * 0.5f;
        else if (vAlign == Alignment::End) pos.y += (r.size.y - size.y);
        BatchCachedGeometry(cache, pos, color.ToRGBA(), m_Font->GetTexture());
    }

    void DrawImage(const Rect& rect, U32 texture, const Math::Vec4& uv01, const Color& tint = Color::White) {
        AddQuad(rect, tint, uv01, texture);
    }

    void SetFont(std::shared_ptr<UIFont> font) { m_Font = std::move(font); }
    IGraphicsContext* GetGraphics() { return m_Graphics; }

private:
    void Initialize() {
        CreatePipeline();
        CreateBuffers();
        U8 w[4]{255, 255, 255, 255};
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
        m_VertexBuffer = m_Graphics->CreateVertexBuffer(nullptr, MAX_VERTICES * sizeof(UIVertex));
        m_IndexBuffer = m_Graphics->CreateIndexBuffer(nullptr, MAX_INDICES * sizeof(U32));
    }

    void AddQuad(const Rect& rect, const Color& color, const Math::Vec4& uv, U32 texture) {
        if (m_CurrentTexture != texture && m_CurrentTexture != INVALID_INDEX && !m_Indices.empty()) {
            FlushBatch();
        }
        m_CurrentTexture = texture;
        if (m_Vertices.size() + 4 > MAX_VERTICES || m_Indices.size() + 6 > MAX_INDICES) {
            FlushBatch();
        }
        U32 base{static_cast<U32>(m_Vertices.size())};
        U32 col{color.ToRGBA()};
        m_Vertices.push_back({{rect.Left(), rect.Top()}, {uv.x, uv.y}, col});
        m_Vertices.push_back({{rect.Right(), rect.Top()}, {uv.z, uv.y}, col});
        m_Vertices.push_back({{rect.Right(), rect.Bottom()}, {uv.z, uv.w}, col});
        m_Vertices.push_back({{rect.Left(), rect.Bottom()}, {uv.x, uv.w}, col});
        m_Indices.insert(m_Indices.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
    }

    void FlushBatch() {
        if (m_Indices.size() == m_BatchStartIndex) return;
        U32 tex{m_CurrentTexture == INVALID_INDEX ? m_WhiteTex : m_CurrentTexture};
        U32 start{m_BatchStartIndex};
        U32 count{static_cast<U32>(m_Indices.size() - m_BatchStartIndex)};
        m_DrawCalls.push_back({count, start, tex});
        m_BatchStartIndex = static_cast<U32>(m_Indices.size());
        m_CurrentTexture = INVALID_INDEX;
    }

    void GenerateTextGeometry(const std::string& text, U32 fontSize,
                              Vector<UIVertex>& vertices, Vector<U32>& indices) {
        F32 px{static_cast<F32>(fontSize)};
        F32 s{px / m_Font->GetBakePx()};
        F32 cursorX{0.0f};
        F32 baselineY{m_Font->GetAscent() * s};
        U32 prev{0};
        for (size_t i{}; i < text.size();) {
            U32 cp{};
            size_t n{UIFont::DecodeUTF8(text, i, cp)};
            if (n == 0) break;
            i += n;
            if (cp == '\n') {
                cursorX = 0.0f;
                baselineY += (m_Font->GetAscent() - m_Font->GetDescent() + m_Font->GetLineGap()) * s;
                prev = 0;
                continue;
            }
            if (prev) cursorX += m_Font->KerningAtBakePx(prev, cp) * s;
            F32 x0{}, y0{}, x1{}, y1{}, u0{}, v0{}, u1{}, v1{}, adv{};
            m_Font->GetQuad(cp, cursorX / s, 0.0f, x0, y0, x1, y1, u0, v0, u1, v1, adv);
            U32 base{static_cast<U32>(vertices.size())};
            vertices.push_back({{x0 * s, baselineY + y0 * s}, {u0, v0}, 0xFFFFFFFF});
            vertices.push_back({{x1 * s, baselineY + y0 * s}, {u1, v0}, 0xFFFFFFFF});
            vertices.push_back({{x1 * s, baselineY + y1 * s}, {u1, v1}, 0xFFFFFFFF});
            vertices.push_back({{x0 * s, baselineY + y1 * s}, {u0, v1}, 0xFFFFFFFF});
            indices.insert(indices.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
            cursorX += adv * s;
            prev = cp;
        }
    }

    void BatchCachedGeometry(const VertexCache& cache, const Math::Vec2& offset, U32 color, U32 texture) {
        if (m_CurrentTexture != texture && m_CurrentTexture != INVALID_INDEX && !m_Indices.empty()) {
            FlushBatch();
        }
        m_CurrentTexture = texture;
        if (m_Vertices.size() + cache.vertices.size() > MAX_VERTICES ||
            m_Indices.size() + cache.indices.size() > MAX_INDICES) {
            FlushBatch();
        }
        U32 base{static_cast<U32>(m_Vertices.size())};
        for (auto const& v : cache.vertices) {
            m_Vertices.push_back({{v.position.x + offset.x, v.position.y + offset.y}, v.uv, color});
        }
        for (U32 idx : cache.indices) {
            m_Indices.push_back(base + idx);
        }
    }
};

void UIPanel::OnDraw(UIRenderer* renderer) {
    if (m_BackgroundColor.a > 0.01f) {
        renderer->DrawRect(m_WorldRect, m_BackgroundColor);
    }
    if (m_BorderWidth > 0 && m_BorderColor.a > 0.01f) {
        renderer->DrawRectOutline(m_WorldRect, m_BorderColor, m_BorderWidth);
    }
}

void UIText::OnDraw(UIRenderer* renderer) {
    renderer->DrawText(m_Text, m_WorldRect, m_FontSize, m_TextColor, m_HorizontalAlign, m_VerticalAlign);
}

void UIImage::OnDraw(UIRenderer* renderer) {
    if (m_Texture != INVALID_INDEX) {
        renderer->DrawImage(m_WorldRect, m_Texture, m_UV, m_Tint);
    }
}