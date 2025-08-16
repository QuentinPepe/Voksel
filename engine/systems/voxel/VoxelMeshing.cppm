export module Systems.VoxelMeshing;

import ECS.Component;
import ECS.SystemScheduler;
import ECS.World;
import Graphics;
import Components.Voxel;
import Components.VoxelStreaming;
import Systems.CameraManager;
import Components.Transform;
import Core.Types;
import Core.Assert;
import Math.Vector;
import std;

namespace detail {
    inline void PushTri(Vector<Vertex>& out, Math::Vec3 const& a, Math::Vec3 const& b, Math::Vec3 const& c, Math::Vec3 const& n, Math::Vec2 const& uva, Math::Vec2 const& uvb, Math::Vec2 const& uvc, U32 mat) {
        Vertex va{}; va.position[0]=a.x; va.position[1]=a.y; va.position[2]=a.z; va.normal[0]=n.x; va.normal[1]=n.y; va.normal[2]=n.z; va.uv[0]=uva.x; va.uv[1]=uva.y; va.material=mat;
        Vertex vb{}; vb.position[0]=b.x; vb.position[1]=b.y; vb.position[2]=b.z; vb.normal[0]=n.x; vb.normal[1]=n.y; vb.normal[2]=n.z; vb.uv[0]=uvb.x; vb.uv[1]=uvb.y; vb.material=mat;
        Vertex vc{}; vc.position[0]=c.x; vc.position[1]=c.y; vc.position[2]=c.z; vc.normal[0]=n.x; vc.normal[1]=n.y; vc.normal[2]=n.z; vc.uv[0]=uvc.x; vc.uv[1]=uvc.y; vc.material=mat;
        out.push_back(va); out.push_back(vb); out.push_back(vc);
    }
    inline void AddFace(Vector<Vertex>& out, Math::Vec3 const& p0, Math::Vec3 const& p1, Math::Vec3 const& p2, Math::Vec3 const& p3, Math::Vec3 const& n, Math::Vec2 const& uv0, Math::Vec2 const& uv1, Math::Vec2 const& uv2, Math::Vec2 const& uv3, U32 mat, bool cw) {
        if (cw) { PushTri(out, p0, p1, p2, n, uv0, uv1, uv2, mat); PushTri(out, p0, p2, p3, n, uv0, uv2, uv3, mat); }
        else { PushTri(out, p3, p2, p1, n, uv3, uv2, uv1, mat); PushTri(out, p3, p1, p0, n, uv3, uv1, uv0, mat); }
    }
}

namespace {
    enum Face : U32 { NX, PX, NY, PY, NZ, PZ };

    constexpr U32 TILE_DIRT{0u};
    constexpr U32 TILE_GRASS_SIDE{1u};
    constexpr U32 TILE_GRASS_TOP{2u};
    constexpr U32 TILE_STONE{3u};
    constexpr U32 TILE_LOG{4u};
    constexpr U32 TILE_LOG_TOP{5u};
    constexpr U32 TILE_LEAVES{6u};

    struct MaterialDef { std::array<U32, 6> face; };

    constexpr std::array<U32, 6> MakeBlock(U32 tile) { return {tile, tile, tile, tile, tile, tile}; }
    constexpr std::array<U32, 6> MakeBlock(U32 tile, U32 tileTopAndBottom) { return {tile, tile, tileTopAndBottom, tileTopAndBottom, tile, tile}; }
    constexpr std::array<U32, 6> MakeBlock(U32 tileTop, U32 tileBottom, U32 tileSide) { return {tileSide, tileSide, tileBottom, tileTop, tileSide, tileSide}; }

    constexpr MaterialDef kMatLUT[]{
        {{0,0,0,0,0,0}},
        {MakeBlock(TILE_DIRT)},
        {{MakeBlock(TILE_GRASS_TOP, TILE_DIRT, TILE_GRASS_SIDE)}},
        {{MakeBlock(TILE_STONE)}},
        {{MakeBlock(TILE_LOG, TILE_LOG_TOP)}},
        {{MakeBlock(TILE_LEAVES)}},
    };

    inline U32 FaceOf(S32 axis, bool back) {
        if (axis == 0) return back ? NX : PX;
        if (axis == 1) return back ? NY : PY;
        return back ? NZ : PZ;
    }

    inline U64 PackKey(S32 x, S32 y, S32 z) {
        constexpr U64 B{1ull << 20};
        return (static_cast<U64>(static_cast<S64>(x) + static_cast<S64>(B)))
             | (static_cast<U64>(static_cast<S64>(y) + static_cast<S64>(B)) << 21)
             | (static_cast<U64>(static_cast<S64>(z) + static_cast<S64>(B)) << 42);
    }

    // key packing: 0 = empty, otherwise (tile+1) | BACK_BIT
    constexpr U32 BACK_BIT{0x80000000u};
    inline U32 PackMask(U32 tile, bool back) { return (tile + 1u) | (back ? BACK_BIT : 0u); }
    inline bool MaskBack(U32 key) { return (key & BACK_BIT) != 0u; }
    inline U32 MaskTile(U32 key) { return (key & ~BACK_BIT) - 1u; }
}

export class VoxelMeshingSystem : public System<VoxelMeshingSystem> {
public:
    void Setup() {
        SetName("VoxelMeshing");
        SetStage(SystemStage::PreRender);
        SetPriority(SystemPriority::High);
        RunBefore("VoxelUpload");
        SetParallel(false);
    }

    void Run(World* world, F32) override {
        auto* cfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(cfgStore && cfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* cfg{}; for (auto [h,c] : *cfgStore) { cfg = &c; break; }

        auto* scStore{world->GetStorage<VoxelStreamingConfig>()};
        assert(scStore && scStore->Size() > 0, "Missing VoxelStreamingConfig");
        VoxelStreamingConfig const* sc{}; for (auto [h,c] : *scStore) { sc = &c; break; }

        auto* chunkStore{world->GetStorage<VoxelChunk>()};
        if (!chunkStore) return;

        const USize kCount{static_cast<USize>(VoxelChunk::SizeX) * VoxelChunk::SizeY * VoxelChunk::SizeZ};

        UnorderedMap<U64, VoxelChunk const*> chunkMap{};
        chunkMap.reserve(chunkStore->Size());
        for (auto [h,c] : *chunkStore) {
            if (c.blocks.size() != kCount) continue;
            U64 k{PackKey(static_cast<S32>(c.cx), static_cast<S32>(c.cy), static_cast<S32>(c.cz))};
            chunkMap.emplace(k, &c);
        }

        Math::Vec3 camPos{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto* t{world->GetComponent<Transform>(h)}) camPos = t->position;
        }

        const F32 sx{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeX)};
        const F32 sy{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeY)};
        const F32 sz{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeZ)};

        struct Item { F32 d2; EntityHandle h; };
        Vector<Item> dirty{};
        dirty.reserve(chunkStore->Size());
        for (auto [h,c] : *chunkStore) {
            if (!c.dirty || c.generating || c.blocks.size() != kCount) continue;
            Math::Vec3 center{c.origin.x + 0.5f * sx, c.origin.y + 0.5f * sy, c.origin.z + 0.5f * sz};
            dirty.push_back(Item{(center - camPos).LengthSquared(), h});
        }

        if (dirty.empty()) return;
        std::ranges::sort(dirty, {}, &Item::d2);

        U32 left{sc->meshBudget};
        for (auto const& it : dirty) {
            if (left == 0u) break;

            auto* mesh{world->GetComponent<VoxelMesh>(it.h)};
            auto const* chunk{world->GetComponent<VoxelChunk>(it.h)};
            if (!mesh || !chunk || !chunk->dirty) continue;
            assert(chunk->blocks.size() == kCount, "Chunk must be generated");

            mesh->cpuVertices.clear();

            const F32 s{cfg->blockSize};
            const S32 NX{static_cast<S32>(VoxelChunk::SizeX)};
            const S32 NY{static_cast<S32>(VoxelChunk::SizeY)};
            const S32 NZ{static_cast<S32>(VoxelChunk::SizeZ)};

            U32 reserveCount{12u * static_cast<U32>(NX*NY + NY*NZ + NX*NZ)};
            mesh->cpuVertices.reserve(reserveCount);

            VoxelChunk const* nbChunk[3][3][3]{};
            Voxel const* nbData[3][3][3]{};
            for (S32 dz{-1}; dz<=1; ++dz) {
                for (S32 dy{-1}; dy<=1; ++dy) {
                    for (S32 dx{-1}; dx<=1; ++dx) {
                        S32 ccx{static_cast<S32>(chunk->cx) + dx};
                        S32 ccy{static_cast<S32>(chunk->cy) + dy};
                        S32 ccz{static_cast<S32>(chunk->cz) + dz};
                        auto itn{chunkMap.find(PackKey(ccx,ccy,ccz))};
                        VoxelChunk const* ch{(itn==chunkMap.end()? nullptr : itn->second)};
                        nbChunk[dx+1][dy+1][dz+1] = ch;
                        nbData[dx+1][dy+1][dz+1] = ch ? ch->blocks.data() : nullptr;
                    }
                }
            }

            auto sample = [&](S32 lx, S32 ly, S32 lz) -> Voxel {
                S32 nx{0}, ny{0}, nz{0};
                if (lx < 0) { nx = -1; lx += NX; } else if (lx >= NX) { nx = 1; lx -= NX; }
                if (ly < 0) { ny = -1; ly += NY; } else if (ly >= NY) { ny = 1; ly -= NY; }
                if (lz < 0) { nz = -1; lz += NZ; } else if (lz >= NZ) { nz = 1; lz -= NZ; }
                Voxel const* data{nbData[nx+1][ny+1][nz+1]};
                if (!data) return Voxel::Air;
                return data[VoxelIndex(static_cast<U32>(lx), static_cast<U32>(ly), static_cast<U32>(lz))];
            };

            auto makePos = [&](S32 gx, S32 gy, S32 gz) -> Math::Vec3 {
                const S64 Cx{static_cast<S64>(static_cast<S32>(chunk->cx))};
                const S64 Cy{static_cast<S64>(static_cast<S32>(chunk->cy))};
                const S64 Cz{static_cast<S64>(static_cast<S32>(chunk->cz))};
                const F64 ds{static_cast<F64>(s)};
                const F64 X{(Cx * static_cast<S64>(NX) + static_cast<S64>(gx)) * ds};
                const F64 Y{(Cy * static_cast<S64>(NY) + static_cast<S64>(gy)) * ds};
                const F64 Z{(Cz * static_cast<S64>(NZ) + static_cast<S64>(gz)) * ds};
                return Math::Vec3{static_cast<F32>(X), static_cast<F32>(Y), static_cast<F32>(Z)};
            };

            // Greedy meshing with packed mask (branch-minimized)
            auto greedyAxis = [&](S32 d) {
                S32 dims[3]{NX,NY,NZ};
                S32 u{(d + 1) % 3};
                S32 v{(d + 2) % 3};

                struct MaskRow { U32* ptr; };
                static thread_local Vector<U32> mask;
                USize maskSize{static_cast<USize>(dims[u] * dims[v])};
                if (mask.size() < maskSize) mask.resize(maskSize);
                std::fill(mask.begin(), mask.begin() + maskSize, 0u);

                S32 x[3]{0,0,0};
                for (x[d]=0; x[d] <= dims[d]; ++x[d]) {
                    U32* m{mask.data()};
                    for (x[v]=0; x[v] < dims[v]; ++x[v]) {
                        for (x[u]=0; x[u] < dims[u]; ++x[u]) {
                            S32 ax{x[0]}, ay{x[1]}, az{x[2]};
                            S32 bx{ax}, by{ay}, bz{az};
                            if (d==0) --ax; else if (d==1) --ay; else --az;

                            Voxel va{sample(ax,ay,az)};
                            Voxel vb{sample(bx,by,bz)};
                            bool sa{va != Voxel::Air};
                            bool sb{vb != Voxel::Air};

                            if (sa != sb) {
                                bool back{!sa};
                                Voxel owner{back ? vb : va};
                                U32 face{FaceOf(d, back)};
                                U32 tile{kMatLUT[static_cast<U32>(owner)].face[face]};
                                *m = PackMask(tile, back);
                            } else {
                                *m = 0u;
                            }
                            ++m;
                        }
                    }

                    S32 j{};
                    while (j < dims[v]) {
                        S32 i{};
                        while (i < dims[u]) {
                            U32 key{mask[static_cast<USize>(i + j * dims[u])]};
                            if (key == 0u) { ++i; continue; }

                            S32 w{1};
                            while (i + w < dims[u]) {
                                if (mask[static_cast<USize>(i + w + j * dims[u])] != key) break;
                                ++w;
                            }

                            S32 h{1}; bool extend{true};
                            while (j + h < dims[v] && extend) {
                                for (S32 k{}; k < w; ++k) {
                                    if (mask[static_cast<USize>(i + k + (j + h) * dims[u])] != key) { extend = false; break; }
                                }
                                if (extend) ++h;
                            }

                            S32 u0{i}, v0{j}, u1{i+w}, v1{j+h};
                            S32 a0[3]{}, a1[3]{}, a2[3]{}, a3[3]{};
                            a0[d]=x[d]; a0[u]=u0; a0[v]=v0;
                            a1[d]=x[d]; a1[u]=u0; a1[v]=v1;
                            a2[d]=x[d]; a2[u]=u1; a2[v]=v1;
                            a3[d]=x[d]; a3[u]=u1; a3[v]=v0;

                            Math::Vec3 p0{makePos(a0[0],a0[1],a0[2])};
                            Math::Vec3 p1{makePos(a1[0],a1[1],a1[2])};
                            Math::Vec3 p2{makePos(a2[0],a2[1],a2[2])};
                            Math::Vec3 p3{makePos(a3[0],a3[1],a3[2])};

                            Math::Vec3 nrm{};
                            if (d==0) nrm = MaskBack(key) ? Math::Vec3{-1,0,0} : Math::Vec3{+1,0,0};
                            if (d==1) nrm = MaskBack(key) ? Math::Vec3{0,-1,0} : Math::Vec3{0,+1,0};
                            if (d==2) nrm = MaskBack(key) ? Math::Vec3{0,0,-1} : Math::Vec3{0,0,+1};

                            bool cw{!MaskBack(key)};
                            Math::Vec2 t0{}, t1{}, t2{}, t3{};
                            if (d == 0 || d == 1) {
                                t0 = {0.0f, 0.0f};
                                t1 = {static_cast<F32>(h), 0.0f};
                                t2 = {static_cast<F32>(h), static_cast<F32>(w)};
                                t3 = {0.0f, static_cast<F32>(w)};
                            } else {
                                t0 = {0.0f, 0.0f};
                                t1 = {0.0f, static_cast<F32>(h)};
                                t2 = {static_cast<F32>(w), static_cast<F32>(h)};
                                t3 = {static_cast<F32>(w), 0.0f};
                            }

                            detail::AddFace(mesh->cpuVertices, p0,p1,p2,p3, nrm, t0,t1,t2,t3, MaskTile(key), cw);

                            for (S32 y{}; y < h; ++y) {
                                USize base{static_cast<USize>((j + y) * dims[u] + i)};
                                std::fill_n(mask.data() + base, static_cast<USize>(w), 0u);
                            }
                            i += w;
                        }
                        ++j;
                    }
                }
            };

            greedyAxis(0); greedyAxis(1); greedyAxis(2);

            mesh->vertexCount = static_cast<U32>(mesh->cpuVertices.size());
            mesh->gpuDirty = true;
            const_cast<VoxelChunk*>(chunk)->dirty = false;
            --left;
        }
    }
};
