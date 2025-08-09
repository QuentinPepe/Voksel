export module Systems.VoxelSelectionRender;

import ECS.World;
import ECS.SystemScheduler;
import Components.Voxel;
import Components.VoxelSelection;
import Systems.CameraManager;
import Components.Camera;
import Components.Transform;
import Graphics;
import Graphics.RenderData;
import Math.Matrix;
import Math.Vector;
import Core.Types;
import Core.Assert;
import std;

static inline void BuildCubeTris(Vector<Math::Vec3>& pos, Vector<U32>& idx) {
    pos = {
        {-0.5f,-0.5f,-0.5f},{+0.5f,-0.5f,-0.5f},{+0.5f,+0.5f,-0.5f},{-0.5f,+0.5f,-0.5f},
        {-0.5f,-0.5f,+0.5f},{+0.5f,-0.5f,+0.5f},{+0.5f,+0.5f,+0.5f},{-0.5f,+0.5f,+0.5f}
    };
    U32 e[]{0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,3,2,6,3,6,7,1,5,6,1,6,2,0,3,7,0,7,4};
    idx.assign(e, e + 36);
}

export class VoxelSelectionRenderSystem : public System<VoxelSelectionRenderSystem> {
private:
    IGraphicsContext* m_Gfx{nullptr};
    U32 m_Pipeline{INVALID_INDEX};
    U32 m_VB{INVALID_INDEX};
    U32 m_IB{INVALID_INDEX};
    U32 m_CameraCB{INVALID_INDEX};
    U32 m_ObjectCB{INVALID_INDEX};
    U32 m_VCount{0};
    U32 m_ICount{0};

public:
    void Setup() {
        SetName("VoxelSelectionRender");
        SetStage(SystemStage::Render);
        SetPriority(SystemPriority::High);
        SetParallel(false);
    }

    void SetGraphicsContext(IGraphicsContext* gfx) { m_Gfx = gfx; }

    void Run(World* world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");
        EnsureResources();

        VoxelSelection sel{};
        if (auto* s{world->GetStorage<VoxelSelection>()}) {
            for (auto [h,c] : *s) { sel = c; break; }
        }
        if (!sel.valid) return;

        auto* wcfg{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfg && wcfg->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* cfg{};
        for (auto [h,c] : *wcfg) { cfg = &c; break; }
        F32 const bs{cfg->blockSize};

        CameraConstants cam{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto* c{world->GetComponent<Camera>(h)}) {
                cam.view = c->view;
                cam.projection = c->projection;
                cam.viewProjection = c->viewProjection;
            }
            if (auto* t{world->GetComponent<Transform>(h)}) { cam.cameraPosition = t->position; }
        }
        m_Gfx->UpdateConstantBuffer(m_CameraCB, &cam, sizeof(cam));

        Math::Mat4 S{Math::Mat4::Scale(bs * 1.01f, bs * 1.01f, bs * 1.01f)};
        Math::Mat4 T{Math::Mat4::Translation(
            (static_cast<F32>(sel.gx) + 0.5f) * bs,
            (static_cast<F32>(sel.gy) + 0.5f) * bs,
            (static_cast<F32>(sel.gz) + 0.5f) * bs
        )};
        ObjectConstants obj{};
        obj.world = T * S;
        m_Gfx->UpdateConstantBuffer(m_ObjectCB, &obj, sizeof(obj));

        m_Gfx->SetPipeline(m_Pipeline);
        m_Gfx->SetConstantBuffer(m_CameraCB, 0);
        m_Gfx->SetConstantBuffer(m_ObjectCB, 1);
        m_Gfx->SetVertexBuffer(m_VB);
        m_Gfx->SetIndexBuffer(m_IB);
        m_Gfx->DrawIndexed(m_ICount);
    }

private:
    void EnsureResources() {
        if (m_Pipeline == INVALID_INDEX) {
            GraphicsPipelineCreateInfo pi{};
            ShaderCode vs{};
            vs.source =
                R"(cbuffer CameraCB : register(b0){ float4x4 gView; float4x4 gProj; float4x4 gViewProj; float3 gCamPos; float _pad; }
                   cbuffer ObjectCB : register(b1){ float4x4 gWorld; }
                   struct VSIn { float3 pos : POSITION; };
                   struct VSOut{ float4 pos : SV_Position; };
                   VSOut VSMain(VSIn i){
                       VSOut o;
                       float4 p = mul(float4(i.pos,1), gWorld);
                       float4 clip = mul(p, gViewProj);
                       clip.z -= 1e-4f * clip.w;
                       o.pos = clip;
                       return o;
                   })";
            vs.entryPoint = "VSMain";
            vs.stage = ShaderStage::Vertex;

            ShaderCode ps{};
            ps.source = R"(float4 PSMain() : SV_Target { return float4(1.0,1.0,0.0,0.35); })";
            ps.entryPoint = "PSMain";
            ps.stage = ShaderStage::Pixel;

            pi.shaders = {vs, ps};
            pi.vertexAttributes = {{"POSITION", 0}};
            pi.vertexStride = static_cast<U32>(sizeof(Math::Vec3));
            pi.topology = PrimitiveTopology::TriangleList;
            pi.depthTest = true;
            pi.depthWrite = false;
            m_Pipeline = m_Gfx->CreateGraphicsPipeline(pi);
        }

        if (m_VB == INVALID_INDEX) {
            Vector<Math::Vec3> pos{};
            Vector<U32> idx{};
            BuildCubeTris(pos, idx);
            m_VCount = static_cast<U32>(pos.size());
            m_ICount = static_cast<U32>(idx.size());
            m_VB = m_Gfx->CreateVertexBuffer(pos.data(), static_cast<U64>(pos.size() * sizeof(Math::Vec3)));
            m_IB = m_Gfx->CreateIndexBuffer(idx.data(), static_cast<U64>(idx.size() * sizeof(U32)));
        }

        if (m_CameraCB == INVALID_INDEX) m_CameraCB = m_Gfx->CreateConstantBuffer(sizeof(CameraConstants));
        if (m_ObjectCB == INVALID_INDEX) m_ObjectCB = m_Gfx->CreateConstantBuffer(sizeof(ObjectConstants));
        assert(sizeof(Math::Vec3) == 12 || sizeof(Math::Vec3) == 16, "Unexpected Vec3 size");
    }
};
