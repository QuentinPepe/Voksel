export module Systems.VoxelRenderer;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Voxel;
import Components.Camera;
import Components.Transform;
import Systems.CameraManager;
import Graphics;
import Graphics.RenderData;
import Core.Types;
import Core.Assert;
import Math.Matrix;
import std;

export class VoxelRendererSystem : public System<VoxelRendererSystem> {
private:
    IGraphicsContext* m_Gfx{nullptr};
    U32 m_CameraCB{INVALID_INDEX};
    U32 m_ObjectCB{INVALID_INDEX};

public:
    void Setup() {
        SetName("VoxelRenderer");
        SetStage(SystemStage::Render);
        SetPriority(SystemPriority::Normal);
        SetParallel(false);
    }

    void SetGraphicsContext(IGraphicsContext* gfx) { m_Gfx = gfx; }

    U32 CreatePipeline() const {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        ShaderCode vs{}; vs.source = "voxel\\Voxel.hlsl"; vs.entryPoint = "VSMain"; vs.stage = ShaderStage::Vertex;
        ShaderCode ps{}; ps.source = "voxel\\Voxel.hlsl"; ps.entryPoint = "PSMain"; ps.stage = ShaderStage::Pixel;

        GraphicsPipelineCreateInfo pi{};
        pi.shaders = {vs, ps};
        pi.vertexAttributes = {{"POSITION", 0}, {"COLOR", 12}};
        pi.vertexStride = sizeof(Vertex);
        pi.topology = PrimitiveTopology::TriangleList;
        pi.depthTest = true;
        pi.depthWrite = true;

        return m_Gfx->CreateGraphicsPipeline(pi);
    }

    void Run(World* world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        auto* rr{world->GetStorage<VoxelRenderResources>()};
        if (!rr || rr->Size() == 0) {
            auto e{world->CreateEntity()};
            VoxelRenderResources r{}; r.pipeline = CreatePipeline();
            world->AddComponent(e, r);
            rr = world->GetStorage<VoxelRenderResources>();
        }

        const VoxelRenderResources* res{}; for (auto [h, r] : *rr) { res = &r; break; }
        assert(res != nullptr && res->pipeline != INVALID_INDEX, "Invalid render resources");

        if (m_CameraCB == INVALID_INDEX) m_CameraCB = m_Gfx->CreateConstantBuffer(sizeof(CameraConstants));
        if (m_ObjectCB  == INVALID_INDEX) m_ObjectCB  = m_Gfx->CreateConstantBuffer(sizeof(ObjectConstants));

        CameraConstants cam{};
        if (auto h = CameraManager::GetPrimaryCamera(); h.valid()) {
            if (auto* camCmp = world->GetComponent<Camera>(h)) {
                cam.view           = camCmp->view;
                cam.projection     = camCmp->projection;
                cam.viewProjection = camCmp->viewProjection;
            }
            if (auto* tr = world->GetComponent<Transform>(h)) {
                cam.cameraPosition = tr->position;
            }
        }
        m_Gfx->UpdateConstantBuffer(m_CameraCB, &cam, sizeof(cam));

        ObjectConstants obj{};
        obj.world = Math::Mat4::Identity;
        m_Gfx->UpdateConstantBuffer(m_ObjectCB, &obj, sizeof(obj));

        auto* storage{world->GetStorage<VoxelMesh>()};
        if (!storage) { return; }

        m_Gfx->SetPipeline(res->pipeline);
        m_Gfx->SetConstantBuffer(m_CameraCB, 0);
        m_Gfx->SetConstantBuffer(m_ObjectCB,  1);

        for (auto [handle, mesh] : *storage) {
            if (mesh.vertexBuffer == INVALID_INDEX || mesh.vertexCount == 0) { continue; }
            m_Gfx->SetVertexBuffer(mesh.vertexBuffer);
            m_Gfx->Draw(mesh.vertexCount);
        }
    }
};
