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

    void Run(World* world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        auto* storage{world->GetStorage<VoxelMesh>()};
        if (!storage) { return; }

        for (auto [handle, mesh] : *storage) {
            if (!mesh.gpuDirty) { continue; }
            const U64 vsize{static_cast<U64>(mesh.cpuVertices.size() * sizeof(Vertex))};
            const U64 isize{static_cast<U64>(mesh.cpuIndices.size() * sizeof(U32))};
            if (vsize) { mesh.vertexBuffer = m_Gfx->CreateVertexBuffer(mesh.cpuVertices.data(), vsize); }
            if (isize) { mesh.indexBuffer = m_Gfx->CreateIndexBuffer(mesh.cpuIndices.data(), isize); }
            mesh.gpuDirty = false;
        }
    }
};
