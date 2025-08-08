export module Systems.VoxelGeneration;

import ECS.SystemScheduler;
import ECS.World;
import Components.Voxel;
import Components.VoxelStreaming;
import Systems.CameraManager;
import Components.Transform;
import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import std;

namespace noise {
    inline U32 wang(U32 s) { s = (s ^ 61u) ^ (s >> 16); s *= 9u; s ^= s >> 4; s *= 0x27d4eb2d; s ^= s >> 15; return s; }
    inline F32 rnd(S32 x, S32 y) {
        U32 h{wang(static_cast<U32>(x) * 73856093u ^ static_cast<U32>(y) * 19349663u)};
        return static_cast<F32>(h) / static_cast<F32>(std::numeric_limits<U32>::max());
    }
    inline F32 smooth(F32 t) { return t * t * (3.0f - 2.0f * t); }
    inline F32 value2D(F32 x, F32 y) {
        S32 ix{static_cast<S32>(std::floor(x))};
        S32 iy{static_cast<S32>(std::floor(y))};
        F32 fx{x - static_cast<F32>(ix)};
        F32 fy{y - static_cast<F32>(iy)};
        F32 v00{rnd(ix, iy)};
        F32 v10{rnd(ix + 1, iy)};
        F32 v01{rnd(ix, iy + 1)};
        F32 v11{rnd(ix + 1, iy + 1)};
        F32 sx{smooth(fx)}, sy{smooth(fy)};
        F32 a{std::lerp(v00, v10, sx)};
        F32 b{std::lerp(v01, v11, sx)};
        return std::lerp(a, b, sy);
    }
    inline F32 fbm2D(F32 x, F32 y, U32 oct, F32 gain, F32 lacunarity) {
        F32 a{1.0f}, f{1.0f}, sum{0.0f}, norm{0.0f};
        for (U32 i{}; i < oct; ++i) { sum += a * value2D(x * f, y * f); norm += a; a *= gain; f *= lacunarity; }
        return sum / std::max(0.0001f, norm);
    }
}

export class VoxelGenerationSystem : public System<VoxelGenerationSystem> {
public:
    void Setup() {
        SetName("VoxelGeneration");
        SetStage(SystemStage::Update);
        SetPriority(SystemPriority::High);
        SetParallel(false);
        RunBefore("VoxelMeshing");
    }

    void Run(World* world, F32) override {
        auto* wcfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfgStore && wcfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* cfg{}; for (auto [h,c] : *wcfgStore) { cfg = &c; break; }

        auto* scStore{world->GetStorage<VoxelStreamingConfig>()};
        assert(scStore && scStore->Size() > 0, "Missing VoxelStreamingConfig");
        VoxelStreamingConfig const* sc{}; for (auto [h,c] : *scStore) { sc = &c; break; }

        Math::Vec3 camPos{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto* t{world->GetComponent<Transform>(h)}) camPos = t->position;
        }

        auto* chunkStore{world->GetStorage<VoxelChunk>()};
        if (!chunkStore) return;

        struct Item { F32 d2; EntityHandle h; };
        Vector<Item> todo{};

        const F32 sx{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeX)};
        const F32 sy{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeY)};
        const F32 sz{cfg->blockSize * static_cast<F32>(VoxelChunk::SizeZ)};

        for (auto [h,c] : *chunkStore) {
            if (!c.blocks.empty()) continue;
            Math::Vec3 center{
                c.origin.x + 0.5f * sx,
                c.origin.y + 0.5f * sy,
                c.origin.z + 0.5f * sz
            };
            F32 d2{(center - camPos).LengthSquared()};
            todo.push_back(Item{d2, h});
        }

        if (todo.empty()) return;
        std::ranges::sort(todo, {}, &Item::d2);

        U32 left{sc->generateBudget};
        for (auto const& it : todo) {
            if (left == 0u) break;
            auto* chunk{world->GetComponent<VoxelChunk>(it.h)};
            if (!chunk || !chunk->blocks.empty()) continue;

            const U32 NX{VoxelChunk::SizeX}, NY{VoxelChunk::SizeY}, NZ{VoxelChunk::SizeZ};
            const F32 bs{cfg->blockSize};
            chunk->blocks.resize(static_cast<USize>(NX) * NY * NZ);

            for (U32 z{}; z < NZ; ++z) {
                for (U32 x{}; x < NX; ++x) {
                    const F32 wx{chunk->origin.x + static_cast<F32>(x) * bs};
                    const F32 wz{chunk->origin.z + static_cast<F32>(z) * bs};
                    const F32 freq{0.025f};
                    const F32 hBase{12.0f};
                    const F32 hAmp{10.0f};
                    F32 n{noise::fbm2D(wx * freq, wz * freq, 4u, 0.5f, 2.0f)};
                    const S32 h{static_cast<S32>(hBase + n * hAmp)};
                    for (U32 y{}; y < NY; ++y) {
                        const S32 gy{static_cast<S32>(chunk->cy * NY) + static_cast<S32>(y)};
                        Voxel v{Voxel::Air};
                        if (gy < h - 3) v = Voxel::Stone;
                        else if (gy < h - 1) v = Voxel::Dirt;
                        else if (gy == h - 1) v = Voxel::Grass;
                        chunk->blocks[VoxelIndex(x, y, z)] = v;
                    }
                }
            }

            chunk->dirty = true;
            --left;
        }
    }
};
