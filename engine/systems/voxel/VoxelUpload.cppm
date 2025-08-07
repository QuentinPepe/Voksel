export module Systems.VoxelUpload;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Voxel;
import Graphics;
import Core.Types;
import Core.Assert;
import std;

export class VoxelUploadSystem : public System<VoxelUploadSystem> {
private:
    IGraphicsContext* m_Gfx{nullptr};

public:
    void Setup() {
        SetName("VoxelUpload");
        SetStage(SystemStage::PreRender);
        SetPriority(SystemPriority::High);
        SetParallel(false);
    }

    void SetGraphicsContext(IGraphicsContext* gfx) { m_Gfx = gfx; }

    void Run(World* world, F32 /*dt*/) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        auto* storage{world->GetStorage<VoxelMesh>()};
        if (!storage) { return; }

        for (auto [handle, mesh] : *storage) {
            if (!mesh.gpuDirty) { continue; }
            const U64 sizeBytes{static_cast<U64>(mesh.cpuVertices.size() * sizeof(Vertex))};
            if (mesh.vertexBuffer == INVALID_INDEX) {
                mesh.vertexBuffer = m_Gfx->CreateVertexBuffer(mesh.cpuVertices.data(), sizeBytes);
            } else {
                mesh.vertexBuffer = m_Gfx->CreateVertexBuffer(mesh.cpuVertices.data(), sizeBytes);
            }
            mesh.gpuDirty = false;
        }
    }
};
