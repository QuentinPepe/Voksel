export module Systems.UI;

import ECS.SystemScheduler;
import ECS.World;
import Graphics;
import Graphics.RenderData;
import UI.Core;
import Core.Types;
import Core.Assert;
import Input.Core;
import Input.Manager;
import Graphics.Window;
import Math.Vector;
import std;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

struct UIInstanceGPU {
    Math::Vec2 pos;
    Math::Vec2 size;
    Math::Vec4 uv;
    U32 color;
    F32 radius;
    U32 flags;
    U32 _pad;
};

export class UISystem : public System<UISystem>, public UIContext::DrawSink {
private:
    static constexpr U32 MAX_INST{1024};

    IGraphicsContext* m_Gfx{nullptr};
    Window* m_Window{nullptr};
    InputManager* m_Input{nullptr};

    U32 m_Pipeline{INVALID_INDEX};
    U32 m_CBCommon{INVALID_INDEX};
    U32 m_CBInstances{INVALID_INDEX};
    U32 m_WhiteTex{INVALID_INDEX};

    struct Batch { U32 texture; U32 first; U32 count; };
    Vector<UIInstanceGPU> m_Inst;
    Vector<Batch> m_Batches;

    UIInput m_UIIn{};
    bool m_LastDown{false};
    F64 m_TimeAcc{0.0};

    using Panel = std::function<void(UIContext&)>;
    Vector<Panel> m_Panels;

public:
    void Setup() {
        SetName("UI");
        SetStage(SystemStage::Render);
        SetPriority(SystemPriority::High);
        RunAfter("VoxelRenderer");
        SetParallel(false);
    }

    void SetGraphicsContext(IGraphicsContext* g){ m_Gfx = g; }
    void SetWindow(Window* w){ m_Window = w; }
    void SetInputManager(InputManager* im){ m_Input = im; }

    void RegisterPanel(Panel cb){ m_Panels.push_back(cb); }

    void Run(World*, F32 dt) override {
        assert(m_Gfx && m_Window, "UI requires gfx and window");
        EnsureResources();

        m_TimeAcc += static_cast<F64>(dt);

        UpdateInput();

        m_Inst.clear();
        m_Batches.clear();

        UIContext ctx{this};
        for (auto& p : m_Panels) p(ctx);

        CameraConstants cc{};
        Math::Vec2 vs{ViewSize()};
        F32 common[4]{vs.x, vs.y, 0.0f, 0.0f};
        m_Gfx->UpdateConstantBuffer(m_CBCommon, common, sizeof(common));

        if (!m_Inst.empty()) {
            USize n{std::min<USize>(m_Inst.size(), MAX_INST)};
            m_Gfx->UpdateConstantBuffer(m_CBInstances, m_Inst.data(), static_cast<U64>(n*sizeof(UIInstanceGPU)));

            m_Gfx->SetPipeline(m_Pipeline);
            m_Gfx->SetConstantBuffer(m_CBCommon, 0);
            m_Gfx->SetConstantBuffer(m_CBInstances, 2);

            for (auto const& b : m_Batches) {
                m_Gfx->SetTexture(b.texture ? b.texture : m_WhiteTex, 0);
                m_Gfx->Draw(6, b.count, 0, b.first);
            }
        }
    }

    Math::Vec2 ViewSize() const override {
        return Math::Vec2{static_cast<F32>(m_Window->GetWidth()), static_cast<F32>(m_Window->GetHeight())};
    }
    void PushQuad(U32 tex, UIRect r, Math::Vec4 uv, U32 color, F32 radius) override {
        if (m_Inst.size() >= MAX_INST) return;
        UIInstanceGPU g{};
        g.pos = Math::Vec2{r.x, r.y};
        g.size = Math::Vec2{r.w, r.h};
        g.uv = uv;
        g.color = color;
        g.radius = radius;
        g.flags = (tex != 0) ? 1u : 0u;

        U32 first{static_cast<U32>(m_Inst.size())};
        m_Inst.push_back(g);

        if (m_Batches.empty() || m_Batches.back().texture != tex) {
            m_Batches.push_back(Batch{tex, first, 1});
        } else {
            ++m_Batches.back().count;
        }
    }
    UIInput const& Input() const override { return m_UIIn; }
    F32 Time01() const override {
        F64 t{std::fmod(m_TimeAcc, 1.0)};
        return static_cast<F32>(t);
    }

private:
    void EnsureResources() {
        if (m_Pipeline == INVALID_INDEX) {
            GraphicsPipelineCreateInfo pi{};
            ShaderCode vs{}; vs.source = "ui\\UI.hlsl"; vs.entryPoint = "VSMain"; vs.stage = ShaderStage::Vertex;
            ShaderCode ps{}; ps.source = "ui\\UI.hlsl"; ps.entryPoint = "PSMain"; ps.stage = ShaderStage::Pixel;
            pi.shaders = {vs, ps};
            pi.vertexAttributes = {};
            pi.vertexStride = 0;
            pi.topology = PrimitiveTopology::TriangleList;
            pi.depthTest = false;
            pi.depthWrite = false;
            m_Pipeline = m_Gfx->CreateGraphicsPipeline(pi);
        }
        if (m_CBCommon == INVALID_INDEX) m_CBCommon = m_Gfx->CreateConstantBuffer(64);
        if (m_CBInstances == INVALID_INDEX) m_CBInstances = m_Gfx->CreateConstantBuffer(MAX_INST * sizeof(UIInstanceGPU));
        if (m_WhiteTex == INVALID_INDEX) {
            U32 px{0xFFFFFFFFu};
            m_WhiteTex = m_Gfx->CreateTexture2D(&px, 1, 1, 1);
        }
    }

    void UpdateInput() {
        assert(m_Input && m_Window, "UI needs InputManager and Window");

        F64 cx{}, cy{};
        glfwGetCursorPos(m_Window->GetGLFWHandle(), &cx, &cy);
        bool down{m_Input->IsMouseButtonPressed(MouseButton::Left)};
        m_UIIn.mx = static_cast<F32>(cx);
        m_UIIn.my = static_cast<F32>(cy);
        m_UIIn.down = down;
        m_UIIn.pressed = (down && !m_LastDown);
        m_UIIn.released = (!down && m_LastDown);
        m_LastDown = down;
    }
};