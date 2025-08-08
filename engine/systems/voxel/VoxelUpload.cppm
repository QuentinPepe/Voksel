export module Systems.VoxelUpload;

import ECS.SystemScheduler;
import ECS.World;
import Components.Voxel;
import Components.VoxelStreaming;
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

    void SetGraphicsContext(IGraphicsContext* gfx) {
        m_Gfx = gfx;
    }

    void Run(World* world, F32) override {
        assert(m_Gfx != nullptr, "GraphicsContext must be set");

        auto* scStore{world->GetStorage<VoxelStreamingConfig>()};
        assert(scStore && scStore->Size() > 0, "Missing VoxelStreamingConfig");
        VoxelStreamingConfig const* sc{};
        for (auto [h,c] : *scStore) { sc = &c; break; }

        auto* storage{world->GetStorage<VoxelMesh>()};
        if (!storage) return;

        U32 left{sc->uploadBudget};
        for (auto [handle, mesh] : *storage) {
            if (!mesh.gpuDirty) continue;
            if (left == 0u) break;

            // Swap ready vertices to CPU buffer
            if (!mesh.readyVertices.empty()) {
                mesh.cpuVertices.swap(mesh.readyVertices);
                mesh.readyVertices.clear();
            }

            // Upload to GPU
            const U64 vsize{static_cast<U64>(mesh.cpuVertices.size() * sizeof(Vertex))};
            const U64 isize{static_cast<U64>(mesh.cpuIndices.size() * sizeof(U32))};

            if (vsize) {
                mesh.vertexBuffer = m_Gfx->CreateVertexBuffer(mesh.cpuVertices.data(), vsize);
            }
            if (isize) {
                mesh.indexBuffer = m_Gfx->CreateIndexBuffer(mesh.cpuIndices.data(), isize);
            }

            mesh.gpuDirty = false;
            --left;
        }
    }
};