export module Systems.VoxelMeshing;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Graphics;
import Components.Voxel;
import Core.Types;
import Core.Assert;
import Math.Vector;
import std;

namespace detail {
    inline void PushTri(Vector<Vertex>& out, const Math::Vec3& a, const Math::Vec3& b, const Math::Vec3& c, U32 packed) {
        Vertex va{}; va.position[0] = a.x; va.position[1] = a.y; va.position[2] = a.z; *reinterpret_cast<U32*>(&va.color[0]) = packed;
        Vertex vb{}; vb.position[0] = b.x; vb.position[1] = b.y; vb.position[2] = b.z; *reinterpret_cast<U32*>(&vb.color[0]) = packed;
        Vertex vc{}; vc.position[0] = c.x; vc.position[1] = c.y; vc.position[2] = c.z; *reinterpret_cast<U32*>(&vc.color[0]) = packed;
        out.push_back(va); out.push_back(vb); out.push_back(vc);
    }

    inline void AddFace(Vector<Vertex>& out, const Math::Vec3& p0, const Math::Vec3& p1, const Math::Vec3& p2, const Math::Vec3& p3, U32 packed) {
        PushTri(out, p0, p1, p2, packed);
        PushTri(out, p0, p2, p3, packed);
    }
}

export class VoxelMeshingSystem : public QuerySystem<VoxelMeshingSystem, Write<VoxelMesh>, Read<VoxelChunk>> {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("VoxelMeshing");
        SetStage(SystemStage::PreRender);
        SetPriority(SystemPriority::High);
        RunBefore("VoxelUpload");
    }

    void Run(World* world, F32) override {
        auto* cfgStore = world->GetStorage<VoxelWorldConfig>();
        assert(cfgStore && cfgStore->Size() > 0, "Missing VoxelWorldConfig");
        const VoxelWorldConfig* cfg{};
        for (auto [h, c] : *cfgStore) { cfg = &c; break; }

        ForEach(world, [cfg](VoxelMesh* mesh, const VoxelChunk* chunk) {
            if (!chunk->dirty) return;
            mesh->cpuVertices.clear();
            mesh->cpuVertices.reserve(64 * 1024);

            const F32 s = cfg->blockSize;

            auto emitBlock = [&](U32 x, U32 y, U32 z, Voxel v) {
                if (v == Voxel::Air) { return; }
                const U32 color{ColorForVoxel(v)};
                const F32 fx{chunk->origin.x + static_cast<F32>(x) * s};
                const F32 fy{chunk->origin.y + static_cast<F32>(y) * s};
                const F32 fz{chunk->origin.z + static_cast<F32>(z) * s};

                Math::Vec3 p000{fx, fy, fz};
                Math::Vec3 p100{fx + s, fy, fz};
                Math::Vec3 p010{fx, fy + s, fz};
                Math::Vec3 p110{fx + s, fy + s, fz};
                Math::Vec3 p001{fx, fy, fz + s};
                Math::Vec3 p101{fx + s, fy, fz + s};
                Math::Vec3 p011{fx, fy + s, fz + s};
                Math::Vec3 p111{fx + s, fy + s, fz + s};

                auto airAt = [&](S32 nx, S32 ny, S32 nz) {
                    if (!InBounds(nx, ny, nz)) { return true; }
                    return chunk->blocks[VoxelIndex(static_cast<U32>(nx), static_cast<U32>(ny), static_cast<U32>(nz))] == Voxel::Air;
                };

                if (airAt(static_cast<S32>(x), static_cast<S32>(y + 1), static_cast<S32>(z))) { detail::AddFace(mesh->cpuVertices, p010, p110, p111, p011, color); }
                if (airAt(static_cast<S32>(x), static_cast<S32>(y - 1), static_cast<S32>(z))) { detail::AddFace(mesh->cpuVertices, p001, p101, p100, p000, color); }
                if (airAt(static_cast<S32>(x + 1), static_cast<S32>(y), static_cast<S32>(z))) { detail::AddFace(mesh->cpuVertices, p101, p111, p110, p100, color); }
                if (airAt(static_cast<S32>(x - 1), static_cast<S32>(y), static_cast<S32>(z))) { detail::AddFace(mesh->cpuVertices, p000, p010, p011, p001, color); }
                if (airAt(static_cast<S32>(x), static_cast<S32>(y), static_cast<S32>(z + 1))) { detail::AddFace(mesh->cpuVertices, p001, p011, p111, p101, color); }
                if (airAt(static_cast<S32>(x), static_cast<S32>(y), static_cast<S32>(z - 1))) { detail::AddFace(mesh->cpuVertices, p100, p110, p010, p000, color); }
            };

            for (U32 z{0}; z < VoxelChunk::SizeZ; ++z) {
                for (U32 y{0}; y < VoxelChunk::SizeY; ++y) {
                    for (U32 x{0}; x < VoxelChunk::SizeX; ++x) {
                        const Voxel v{chunk->blocks[VoxelIndex(x, y, z)]};
                        emitBlock(x, y, z, v);
                    }
                }
            }

            mesh->vertexCount = static_cast<U32>(mesh->cpuVertices.size());
            mesh->gpuDirty = true;
            const_cast<VoxelChunk*>(chunk)->dirty = false;
        });
    }
};
