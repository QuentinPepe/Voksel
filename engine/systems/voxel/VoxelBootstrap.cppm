export module Systems.VoxelBootstrap;

import ECS.SystemScheduler;
import ECS.World;
import ECS.Query;
import Components.Voxel;
import Core.Types;
import Core.Assert;
import Math.Core;
import Math.Vector;
import std;

export class VoxelBootstrapSystem : public System<VoxelBootstrapSystem> {
private:
    bool m_Initialized{false};

public:
    void Setup() {
        SetName("VoxelBootstrap");
        SetStage(SystemStage::PreUpdate);
        SetPriority(SystemPriority::Critical);
        SetParallel(false);
    }

    void Run(World* world, F32 /*dt*/) override {
        if (m_Initialized) { return; }

        auto* cfg{world->GetStorage<VoxelWorldConfig>()};
        assert(cfg != nullptr, "Missing VoxelWorldConfig storage");

        if (cfg->Size() == 0) {
            auto cfgEntity{world->CreateEntity()};
            world->AddComponent(cfgEntity, VoxelWorldConfig{});
        }

        VoxelWorldConfig* worldCfg{};
        for (auto [handle, c] : *world->GetStorage<VoxelWorldConfig>()) { worldCfg = &c; break; }
        assert(worldCfg != nullptr, "Failed to fetch VoxelWorldConfig");
        assert(worldCfg->chunksX > 0 && worldCfg->chunksY > 0 && worldCfg->chunksZ > 0, "Invalid chunk grid");

        for (U32 cz{0}; cz < worldCfg->chunksZ; ++cz) {
            for (U32 cy{0}; cy < worldCfg->chunksY; ++cy) {
                for (U32 cx{0}; cx < worldCfg->chunksX; ++cx) {
                    auto e{world->CreateEntity()};

                    VoxelChunk chunk{};
                    chunk.origin = Math::Vec3{
                        static_cast<F32>(cx * VoxelChunk::SizeX) * worldCfg->blockSize,
                        static_cast<F32>(cy * VoxelChunk::SizeY) * worldCfg->blockSize,
                        static_cast<F32>(cz * VoxelChunk::SizeZ) * worldCfg->blockSize
                    };
                    chunk.blocks.resize(static_cast<USize>(VoxelChunk::SizeX) * VoxelChunk::SizeY * VoxelChunk::SizeZ);
                    std::fill(chunk.blocks.begin(), chunk.blocks.end(), Voxel::Air);
                    chunk.dirty = true;

                    for (U32 z{0}; z < VoxelChunk::SizeZ; ++z) {
                        for (U32 x{0}; x < VoxelChunk::SizeX; ++x) {
                            F32 wx{chunk.origin.x + static_cast<F32>(x) * worldCfg->blockSize};
                            F32 wz{chunk.origin.z + static_cast<F32>(z) * worldCfg->blockSize};
                            F32 h0{std::sin(wx * 0.05f) * 3.0f + std::cos(wz * 0.05f) * 3.0f};
                            S32 height{static_cast<S32>(8 + h0)};
                            for (S32 y{0}; y < static_cast<S32>(VoxelChunk::SizeY); ++y) {
                                Voxel v{Voxel::Air};
                                if (y < height - 3) { v = Voxel::Stone; }
                                else if (y < height - 1) { v = Voxel::Dirt; }
                                else if (y == height - 1) { v = Voxel::Grass; }
                                else { v = Voxel::Air; }
                                chunk.blocks[VoxelIndex(x, static_cast<U32>(y), z)] = v;
                            }
                        }
                    }

                    world->AddComponent(e, std::move(chunk));
                    world->AddComponent(e, VoxelMesh{});
                }
            }
        }

        m_Initialized = true;
    }
};
