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

    void Run(World* world, F32) override {
        auto* wcfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfgStore && wcfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* wcfg{}; for (auto [h,c] : *wcfgStore) { wcfg = &c; break; }

        auto* scStore{world->GetStorage<VoxelStreamingConfig>()};
        if (!scStore || scStore->Size() == 0) {
            auto e{world->CreateEntity()};
            world->AddComponent(e, VoxelStreamingConfig{});
            scStore = world->GetStorage<VoxelStreamingConfig>();
        }
        VoxelStreamingConfig const* sc{}; for (auto [h,c] : *scStore) { sc = &c; break; }
        assert(sc != nullptr, "Missing VoxelStreamingConfig");

        Math::Vec3 camPos{};
        if (auto h{CameraManager::GetPrimaryCamera()}; h.valid()) {
            if (auto* t{world->GetComponent<Transform>(h)}) camPos = t->position;
        }

        const F32 sx{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeX)};
        const F32 sy{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeY)};
        const F32 sz{wcfg->blockSize * static_cast<F32>(VoxelChunk::SizeZ)};
        const S32 ccx{static_cast<S32>(std::floor(camPos.x / sx))};
        const S32 ccy{static_cast<S32>(std::floor(camPos.y / sy))};
        const S32 ccz{static_cast<S32>(std::floor(camPos.z / sz))};

        UnorderedMap<U64, VoxelChunk*> existing{};
        if (auto* store{world->GetStorage<VoxelChunk>()}) {
            for (auto [h,c] : *store) {
                existing.emplace(PackKey(static_cast<S32>(c.cx), static_cast<S32>(c.cy), static_cast<S32>(c.cz)),
                                 const_cast<VoxelChunk*>(&c));
            }
        }

        UnorderedMap<U64, bool> wanted{};
        U32 createLeft{sc->createBudget};
        U32 removeLeft{sc->removeBudget};

        for (S32 dz{-static_cast<S32>(sc->radius)}; dz <= static_cast<S32>(sc->radius); ++dz) {
            for (S32 dx{-static_cast<S32>(sc->radius)}; dx <= static_cast<S32>(sc->radius); ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) > static_cast<S32>(sc->radius)) continue;
                for (S32 dy{sc->minChunkY}; dy <= sc->maxChunkY; ++dy) {
                    const S32 cx{ccx + dx};
                    const S32 cy{ccy + dy};
                    const S32 cz{ccz + dz};
                    const U64 k{PackKey(cx, cy, cz)};
                    wanted.emplace(k, true);

                    if (!existing.contains(k) && createLeft > 0) {
                        auto e{world->CreateEntity()};
                        VoxelChunk chunk{};
                        chunk.cx = static_cast<U32>(cx);
                        chunk.cy = static_cast<U32>(cy);
                        chunk.cz = static_cast<U32>(cz);
                        chunk.origin = Math::Vec3{
                            static_cast<F32>(cx) * sx,
                            static_cast<F32>(cy) * sy,
                            static_cast<F32>(cz) * sz
                        };
                        chunk.blocks.clear();
                        chunk.dirty = false;
                        world->AddComponent(e, std::move(chunk));
                        world->AddComponent(e, VoxelMesh{});
                        --createLeft;
                    }
                }
            }
        }

        if (auto* store{world->GetStorage<VoxelChunk>()}) {
            for (auto [h,c] : *store) {
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
