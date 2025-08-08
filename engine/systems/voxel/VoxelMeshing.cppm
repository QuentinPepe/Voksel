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
    inline void PushTri(Vector<Vertex>& out, const Math::Vec3& a, const Math::Vec3& b, const Math::Vec3& c, const Math::Vec3& n, const Math::Vec2& uva, const Math::Vec2& uvb, const Math::Vec2& uvc, U32 mat) {
        Vertex va{}; va.position[0] = a.x; va.position[1] = a.y; va.position[2] = a.z; va.normal[0]=n.x; va.normal[1]=n.y; va.normal[2]=n.z; va.uv[0]=uva.x; va.uv[1]=uva.y; va.material=mat;
        Vertex vb{}; vb.position[0] = b.x; vb.position[1] = b.y; vb.position[2] = b.z; vb.normal[0]=n.x; vb.normal[1]=n.y; vb.normal[2]=n.z; vb.uv[0]=uvb.x; vb.uv[1]=uvb.y; vb.material=mat;
        Vertex vc{}; vc.position[0] = c.x; vc.position[1] = c.y; vc.position[2] = c.z; vc.normal[0]=n.x; vc.normal[1]=n.y; vc.normal[2]=n.z; vc.uv[0]=uvc.x; vc.uv[1]=uvc.y; vc.material=mat;
        out.push_back(va); out.push_back(vb); out.push_back(vc);
    }
    inline void AddFace(Vector<Vertex>& out, const Math::Vec3& p0, const Math::Vec3& p1, const Math::Vec3& p2, const Math::Vec3& p3, const Math::Vec3& n, const Math::Vec2& uv0, const Math::Vec2& uv1, const Math::Vec2& uv2, const Math::Vec2& uv3, U32 mat, bool cw) {
        if (cw) {
            PushTri(out, p0, p1, p2, n, uv0, uv1, uv2, mat);
            PushTri(out, p0, p2, p3, n, uv0, uv2, uv3, mat);
        } else {
            PushTri(out, p3, p2, p1, n, uv3, uv2, uv1, mat);
            PushTri(out, p3, p1, p0, n, uv3, uv1, uv0, mat);
        }
    }
}

namespace {
    enum Face : U32 { NX, PX, NY, PY, NZ, PZ };
    struct MaterialDef { U32 face[6]; };
    constexpr U32 TILE_DIRT{0u};
    constexpr U32 TILE_GRASS{1u};
    constexpr U32 TILE_STONE{2u};
    constexpr MaterialDef kMatLUT[] {
        {{0,0,0,0,0,0}},
        {{TILE_DIRT,TILE_DIRT,TILE_DIRT,TILE_DIRT,TILE_DIRT,TILE_DIRT}},
        {{TILE_GRASS,TILE_GRASS,TILE_GRASS,TILE_GRASS,TILE_GRASS,TILE_GRASS}},
        {{TILE_STONE,TILE_STONE,TILE_STONE,TILE_STONE,TILE_STONE,TILE_STONE}},
    };
    inline U32 FaceOf(S32 axis, bool back) {
        if (axis == 0) return back ? NX : PX;
        if (axis == 1) return back ? NY : PY;
        return back ? NZ : PZ;
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
        UnorderedMap<U64, const VoxelChunk*> chunkMap;
        auto* chunkStore = world->GetStorage<VoxelChunk>();
        if (chunkStore) {
            for (auto [h, c] : *chunkStore) {
                U64 k = PackKey(static_cast<S32>(c.cx), static_cast<S32>(c.cy), static_cast<S32>(c.cz));
                chunkMap.emplace(k, &c);
            }
        }
        ForEach(world, [cfg, &chunkMap](VoxelMesh* mesh, const VoxelChunk* chunk) {
            if (!chunk->dirty) return;
            mesh->cpuVertices.clear();
            mesh->cpuVertices.reserve(128 * 1024);
            const F32 s = cfg->blockSize;
            const S32 NX = static_cast<S32>(VoxelChunk::SizeX);
            const S32 NY = static_cast<S32>(VoxelChunk::SizeY);
            const S32 NZ = static_cast<S32>(VoxelChunk::SizeZ);
            auto sample = [&](S32 lx, S32 ly, S32 lz) -> Voxel {
                S32 ocx = 0, ocy = 0, ocz = 0;
                while (lx < 0) { lx += NX; --ocx; }
                while (ly < 0) { ly += NY; --ocy; }
                while (lz < 0) { lz += NZ; --ocz; }
                while (lx >= NX) { lx -= NX; ++ocx; }
                while (ly >= NY) { ly -= NY; ++ocy; }
                while (lz >= NZ) { lz -= NZ; ++ocz; }
                S32 ncx = static_cast<S32>(chunk->cx) + ocx;
                S32 ncy = static_cast<S32>(chunk->cy) + ocy;
                S32 ncz = static_cast<S32>(chunk->cz) + ocz;
                auto it = chunkMap.find(PackKey(ncx, ncy, ncz));
                if (it == chunkMap.end()) return Voxel::Air;
                const VoxelChunk* ch = it->second;
                return ch->blocks[VoxelIndex(static_cast<U32>(lx), static_cast<U32>(ly), static_cast<U32>(lz))];
            };
            auto makePos = [&](S32 gx, S32 gy, S32 gz) -> Math::Vec3 {
                return {chunk->origin.x + static_cast<F32>(gx) * s,
                        chunk->origin.y + static_cast<F32>(gy) * s,
                        chunk->origin.z + static_cast<F32>(gz) * s};
            };
            auto greedyAxis = [&](S32 d) {
                S32 dims[3] = {NX, NY, NZ};
                S32 u = (d + 1) % 3;
                S32 v = (d + 2) % 3;
                struct Cell { U32 tile; bool back; bool set; };
                Vector<Cell> mask;
                mask.resize(static_cast<USize>(dims[u] * dims[v]));
                S32 x[3] = {0,0,0};
                for (x[d] = 0; x[d] <= dims[d]; ++x[d]) {
                    S32 n = 0;
                    for (x[v] = 0; x[v] < dims[v]; ++x[v]) {
                        for (x[u] = 0; x[u] < dims[u]; ++x[u]) {
                            S32 ax = x[0], ay = x[1], az = x[2];
                            S32 bx = x[0], by = x[1], bz = x[2];
                            if (d == 0) { ax -= 1; } else if (d == 1) { ay -= 1; } else { az -= 1; }
                            Voxel va = sample(ax, ay, az);
                            Voxel vb = sample(bx, by, bz);
                            bool sa = va != Voxel::Air;
                            bool sb = vb != Voxel::Air;
                            if (sa == sb) {
                                mask[static_cast<USize>(n)] = {0u, false, false};
                            } else {
                                bool back = !sa;
                                Voxel owner = back ? vb : va;
                                U32 face = FaceOf(d, back);
                                U32 tile = kMatLUT[static_cast<U32>(owner)].face[face];
                                mask[static_cast<USize>(n)] = {tile, back, true};
                            }
                            ++n;
                        }
                    }
                    S32 j = 0;
                    while (j < dims[v]) {
                        S32 i = 0;
                        while (i < dims[u]) {
                            Cell c = mask[static_cast<USize>(i + j * dims[u])];
                            if (!c.set) { ++i; continue; }
                            S32 w = 1;
                            while (i + w < dims[u]) {
                                Cell cc = mask[static_cast<USize>(i + w + j * dims[u])];
                                if (!(cc.set && cc.back == c.back && cc.tile == c.tile)) break;
                                ++w;
                            }
                            S32 h = 1;
                            bool extend = true;
                            while (j + h < dims[v] && extend) {
                                for (S32 k = 0; k < w; ++k) {
                                    Cell cc = mask[static_cast<USize>(i + k + (j + h) * dims[u])];
                                    if (!(cc.set && cc.back == c.back && cc.tile == c.tile)) { extend = false; break; }
                                }
                                if (extend) ++h;
                            }
                            S32 u0 = i, v0 = j, u1 = i + w, v1 = j + h;
                            S32 a0[3] = {0,0,0};
                            S32 a1[3] = {0,0,0};
                            S32 a2[3] = {0,0,0};
                            S32 a3[3] = {0,0,0};
                            a0[d] = x[d];     a0[u] = u0; a0[v] = v0;
                            a1[d] = x[d];     a1[u] = u0; a1[v] = v1;
                            a2[d] = x[d];     a2[u] = u1; a2[v] = v1;
                            a3[d] = x[d];     a3[u] = u1; a3[v] = v0;
                            Math::Vec3 p0 = makePos(a0[0], a0[1], a0[2]);
                            Math::Vec3 p1 = makePos(a1[0], a1[1], a1[2]);
                            Math::Vec3 p2 = makePos(a2[0], a2[1], a2[2]);
                            Math::Vec3 p3 = makePos(a3[0], a3[1], a3[2]);
                            Math::Vec3 nrm{0,0,0};
                            if (d == 0) { nrm = c.back ? Math::Vec3{-1,0,0} : Math::Vec3{+1,0,0}; }
                            if (d == 1) { nrm = c.back ? Math::Vec3{0,-1,0} : Math::Vec3{0,+1,0}; }
                            if (d == 2) { nrm = c.back ? Math::Vec3{0,0,-1} : Math::Vec3{0,0,+1}; }
                            Math::Vec2 t0{0.0f, 0.0f};
                            Math::Vec2 t1{0.0f, static_cast<F32>(h)};
                            Math::Vec2 t2{static_cast<F32>(w), static_cast<F32>(h)};
                            Math::Vec2 t3{static_cast<F32>(w), 0.0f};
                            bool cw = c.back ? false : true;
                            detail::AddFace(mesh->cpuVertices, p0, p1, p2, p3, nrm, t0, t1, t2, t3, c.tile, cw);
                            for (S32 y = 0; y < h; ++y) {
                                for (S32 xw = 0; xw < w; ++xw) {
                                    mask[static_cast<USize>(i + xw + (j + y) * dims[u])] = {0u, false, false};
                                }
                            }
                            i += w;
                        }
                        ++j;
                    }
                }
            };
            greedyAxis(0);
            greedyAxis(1);
            greedyAxis(2);
            mesh->vertexCount = static_cast<U32>(mesh->cpuVertices.size());
            mesh->gpuDirty = true;
            const_cast<VoxelChunk*>(chunk)->dirty = false;
        });
    }

private:
    static inline U64 PackKey(S32 x, S32 y, S32 z) {
        constexpr U64 B = 1ull << 20;
        return (static_cast<U64>(static_cast<S64>(x) + static_cast<S64>(B)))
             | (static_cast<U64>(static_cast<S64>(y) + static_cast<S64>(B)) << 21)
             | (static_cast<U64>(static_cast<S64>(z) + static_cast<S64>(B)) << 42);
    }
};
