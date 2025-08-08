export module Systems.VoxelStreaming;

import ECS.SystemScheduler;
import ECS.World;
import ECS.Query;
import Components.Voxel;
import Components.Transform;
import Components.Camera;
import Components.VoxelStreaming;
import Systems.CameraManager;
import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import std;

namespace {
    inline U64 PackKey(S32 x, S32 y, S32 z) {
        constexpr U64 B{1ull << 20};
        return (static_cast<U64>(static_cast<S64>(x) + static_cast<S64>(B)))
               | (static_cast<U64>(static_cast<S64>(y) + static_cast<S64>(B)) << 21)
               | (static_cast<U64>(static_cast<S64>(z) + static_cast<S64>(B)) << 42);
    }
}

export class VoxelStreamingSystem : public System<VoxelStreamingSystem> {
public:
    void Setup() {
        SetName("VoxelStreaming");
        SetStage(SystemStage::PreUpdate);
        SetPriority(SystemPriority::Critical);
        SetParallel(false);
        RunBefore("VoxelGeneration");
        RunBefore("VoxelMeshing");
    }

    void Run(World *world, F32) override {
        auto *wcfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfgStore && wcfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const *wcfg{};
        for (auto [h,c]: *wcfgStore) {
            wcfg = &c;
            break;
        }

        auto *scStore{world->GetStorage<VoxelStreamingConfig>()};
        if (!scStore || scStore->Size() == 0) {
            auto e{world->CreateEntity()};
            world->AddComponent(e, VoxelStreamingConfig{});
            scStore = world->GetStorage<VoxelStreamingConfig>();
        }
        VoxelStreamingConfig const *sc{};
        for (auto [h,c]: *scStore) {
            sc = &c;
            break;
        }
        assert(sc != nullptr, "Missing VoxelStreamingConfig");

        Math::Vec3 camPos{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto *t{world->GetComponent<Transform>(h)}) camPos = t->position;
        }

        const F32 sx{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeX)};
        const F32 sy{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeY)};
        const F32 sz{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeZ)};
        const S32 ccx{static_cast<S32>(std::floor(camPos.x / sx))};
        const S32 ccy{static_cast<S32>(std::floor(camPos.y / sy))};
        const S32 ccz{static_cast<S32>(std::floor(camPos.z / sz))};

        UnorderedMap<U64, VoxelChunk *> existing{};
        if (auto *store{world->GetStorage<VoxelChunk>()}) {
            for (auto [h,c]: *store) {
                existing.emplace(
                    PackKey(static_cast<S32>(c.cx), static_cast<S32>(c.cy), static_cast<S32>(c.cz)),
                    const_cast<VoxelChunk *>(&c)
                );
            }
        }

        UnorderedMap<U64, bool> wanted{};
        U32 createLeft{sc->createBudget};
        U32 removeLeft{sc->removeBudget};

        struct Cand {
            S32 cx;
            S32 cy;
            S32 cz;
            F32 d2;
            U64 key;
        };
        Vector<Cand> cands{};

        for (S32 dz{-static_cast<S32>(sc->radius)}; dz <= static_cast<S32>(sc->radius); ++dz) {
            for (S32 dx{-static_cast<S32>(sc->radius)}; dx <= static_cast<S32>(sc->radius); ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) > static_cast<S32>(sc->radius)) continue;
                for (S32 dy{sc->minChunkY}; dy <= sc->maxChunkY; ++dy) {
                    S32 cx{ccx + dx}, cy{ccy + dy}, cz{ccz + dz};
                    Math::Vec3 center{
                        (static_cast<F32>(cx) + 0.5f) * sx,
                        (static_cast<F32>(cy) + 0.5f) * sy,
                        (static_cast<F32>(cz) + 0.5f) * sz
                    };
                    F32 d2{(center - camPos).LengthSquared()};
                    U64 k{PackKey(cx, cy, cz)};
                    cands.push_back(Cand{cx, cy, cz, d2, k});
                }
            }
        }

        std::ranges::sort(cands, {}, &Cand::d2);

        for (auto const &c: cands) {
            wanted.emplace(c.key, true);
            if (createLeft == 0u) break;
            if (!existing.contains(c.key)) {
                auto e{world->CreateEntity()};
                VoxelChunk chunk{};
                chunk.cx = static_cast<U32>(c.cx);
                chunk.cy = static_cast<U32>(c.cy);
                chunk.cz = static_cast<U32>(c.cz);
                chunk.origin = Math::Vec3{
                    static_cast<F32>(c.cx) * sx,
                    static_cast<F32>(c.cy) * sy,
                    static_cast<F32>(c.cz) * sz
                };
                chunk.blocks.clear();
                chunk.dirty = false;
                world->AddComponent(e, std::move(chunk));
                world->AddComponent(e, VoxelMesh{});
                --createLeft;
            }
        }

        if (auto *store{world->GetStorage<VoxelChunk>()}) {
            for (auto [h,c]: *store) {
                const S32 dx{static_cast<S32>(c.cx) - ccx};
                const S32 dy{static_cast<S32>(c.cy) - ccy};
                const S32 dz{static_cast<S32>(c.cz) - ccz};
                const S32 md{std::max({std::abs(dx), std::abs(dy), std::abs(dz)})};
                if (md > static_cast<S32>(sc->radius + sc->margin) && removeLeft > 0) {
                    world->DestroyEntity(h);
                    --removeLeft;
                }
            }
        }
    }
};
