export module Systems.VoxelGeneration;

import ECS.Component;
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
    inline U32 wang(U32 s) {
        s = (s ^ 61u) ^ (s >> 16);
        s *= 9u;
        s ^= s >> 4;
        s *= 0x27d4eb2d;
        s ^= s >> 15;
        return s;
    }

    inline F32 rnd(S32 x, S32 y) {
        U32 h{wang(static_cast<U32>(x) * 73856093u ^ static_cast<U32>(y) * 19349663u)};
        return static_cast<F32>(h) / static_cast<F32>(std::numeric_limits<U32>::max());
    }

    inline F32 smooth(F32 t) {
        return t * t * (3.0f - 2.0f * t);
    }

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
        for (U32 i{}; i < oct; ++i) {
            sum += a * value2D(x * f, y * f);
            norm += a;
            a *= gain;
            f *= lacunarity;
        }
        return sum / std::max(0.0001f, norm);
    }

    // Generates a simple and balanced biome value
    inline F32 biomeValue(F32 x, F32 z, F32 freq = 0.004f) {
        F32 large = value2D(x * freq, z * freq);           // Large areas
        F32 detail = value2D(x * freq * 3.0f, z * freq * 3.0f) * 0.3f; // Details
        return large + detail;
    }

    // Generates basic terrain height
    inline F32 terrainNoise(F32 x, F32 z, F32 freq = 0.025f) {
        return fbm2D(x * freq, z * freq, 4u, 0.5f, 2.0f);
    }
}

// ===== BIOMES =====
enum class BiomeType : U32 {
    Plains = 0,
    Desert = 1
};

struct BiomeData {
    BiomeType type;
    F32 heightBase;
    F32 heightAmp;
    F32 treeChance;
    Voxel surfaceBlock;
    Voxel fillerBlock;
    bool hasWater;
};

namespace biomes {
    // Defines biome data
    static BiomeData GetBiomeData(BiomeType type) {
        switch (type) {
            case BiomeType::Plains:
                return BiomeData{
                    .type = BiomeType::Plains,
                    .heightBase = 12.0f,
                    .heightAmp = 10.0f,
                    .treeChance = 0.05f,
                    .surfaceBlock = Voxel::Grass,
                    .fillerBlock = Voxel::Dirt,
                    .hasWater = true
                };
            case BiomeType::Desert:
                return BiomeData{
                    .type = BiomeType::Desert,
                    .heightBase = 14.0f,
                    .heightAmp = 6.0f,
                    .treeChance = 0.0f,
                    .surfaceBlock = Voxel::Sand,
                    .fillerBlock = Voxel::Sand,
                    .hasWater = false
                };
        }
        return GetBiomeData(BiomeType::Plains);
    }

    // Determines the primary biome for a position
    static BiomeType GetPrimaryBiome(F32 x, F32 z) {
        F32 biomeNoise = noise::biomeValue(x, z);
        return (biomeNoise > 0.6f) ? BiomeType::Desert : BiomeType::Plains;
    }

    // Calculates the blend factor between biomes
    static F32 GetBiomeBlendFactor(F32 x, F32 z, BiomeType targetBiome) {
        F32 biomeNoise = noise::biomeValue(x, z);

        if (targetBiome == BiomeType::Desert) {
            if (biomeNoise > 0.7f) return 1.0f;
            if (biomeNoise < 0.5f) return 0.0f;
            return (biomeNoise - 0.5f) / 0.2f;
        } else {
            if (biomeNoise < 0.5f) return 1.0f;
            if (biomeNoise > 0.7f) return 0.0f;
            return 1.0f - ((biomeNoise - 0.5f) / 0.2f);
        }
    }

    // Generates the height for a specific biome
    static S32 GenerateBiomeHeight(F32 x, F32 z, const BiomeData& biome) {
        F32 terrainNoise = noise::terrainNoise(x, z);
        return static_cast<S32>(biome.heightBase + terrainNoise * biome.heightAmp);
    }

    // Calculates the final height with biome blending
    static S32 CalculateBlendedHeight(F32 x, F32 z) {
        BiomeType primaryBiome = GetPrimaryBiome(x, z);
        BiomeData primaryData = GetBiomeData(primaryBiome);
        S32 primaryHeight = GenerateBiomeHeight(x, z, primaryData);

        // Check if we are in a transition zone
        BiomeType otherBiome = (primaryBiome == BiomeType::Plains) ? BiomeType::Desert : BiomeType::Plains;
        F32 blendFactor = GetBiomeBlendFactor(x, z, otherBiome);

        if (blendFactor > 0.0f) {
            BiomeData otherData = GetBiomeData(otherBiome);
            S32 otherHeight = GenerateBiomeHeight(x, z, otherData);
            return static_cast<S32>(std::lerp(static_cast<F32>(primaryHeight),
                                            static_cast<F32>(otherHeight),
                                            blendFactor));
        }

        return primaryHeight;
    }
}

struct GenJob {
    EntityHandle h;
    S32 cx;
    S32 cy;
    S32 cz;
    Math::Vec3 origin;
    F32 bs;
};

// ===== TERRAIN GENERATION =====
namespace terrain {
    // Places the base blocks according to the biome
    static void PlaceBaseBlocks(Vector<Voxel>& blocks, U32 x, U32 y, U32 z,
                               S32 globalY, S32 height, const BiomeData& biome) {
        Voxel blockType = Voxel::Air;

        if (globalY < height - 3) {
            blockType = Voxel::Stone;
        } else if (globalY < height - 1) {
            blockType = biome.fillerBlock;
        } else if (globalY == height - 1) {
            blockType = biome.surfaceBlock;
        }

        blocks[VoxelIndex(x, y, z)] = blockType;
    }

    // Generates a heightmap for a chunk
    static void GenerateHeightMap(Vector<S32>& heightMap, Vector<BiomeType>& biomeMap,
                                 const GenJob& job, U32 chunkSizeX, U32 chunkSizeZ) {
        for (U32 z{}; z < chunkSizeZ; ++z) {
            for (U32 x{}; x < chunkSizeX; ++x) {
                F32 wx{job.origin.x + static_cast<F32>(x) * job.bs};
                F32 wz{job.origin.z + static_cast<F32>(z) * job.bs};

                BiomeType biome = biomes::GetPrimaryBiome(wx, wz);
                S32 height = biomes::CalculateBlendedHeight(wx, wz);

                heightMap[x + z * chunkSizeX] = height;
                biomeMap[x + z * chunkSizeX] = biome;
            }
        }
    }

    // Places all terrain blocks
    static void PlaceTerrainBlocks(Vector<Voxel>& blocks, const Vector<S32>& heightMap,
                                  const Vector<BiomeType>& biomeMap, const GenJob& job,
                                  U32 chunkSizeX, U32 chunkSizeY, U32 chunkSizeZ) {
        for (U32 z{}; z < chunkSizeZ; ++z) {
            for (U32 x{}; x < chunkSizeX; ++x) {
                S32 height = heightMap[x + z * chunkSizeX];
                BiomeType biome = biomeMap[x + z * chunkSizeX];
                BiomeData biomeData = biomes::GetBiomeData(biome);

                for (U32 y{}; y < chunkSizeY; ++y) {
                    S32 globalY{job.cy * static_cast<S32>(chunkSizeY) + static_cast<S32>(y)};
                    PlaceBaseBlocks(blocks, x, y, z, globalY, height, biomeData);
                }
            }
        }
    }
}

// ===== VEGETATION =====
struct TreeCandidate {
    S32 x, y, z;
    U32 height;
    BiomeType biome;
};

struct CactusCandidate {
    S32 x, y, z;
    U32 height;
};

namespace vegetation {
    // Finds candidates for vegetation

    // Finds a tree candidate
    static void FindTreeCandidate(Vector<TreeCandidate>& trees, const Vector<S32>& heightMap,
                                 const Vector<Voxel>& blocks, const GenJob& job, U32 seed,
                                 U32 x, U32 z, U32 chunkSizeX, U32 chunkSizeY, BiomeType biome) {
        S32 offsetX{static_cast<S32>(seed % 5) - 2};
        S32 offsetZ{static_cast<S32>((seed >> 8) % 5) - 2};
        S32 treeX{static_cast<S32>(x) + offsetX};
        S32 treeZ{static_cast<S32>(z) + offsetZ};

        if (treeX >= 2 && treeX < static_cast<S32>(chunkSizeX) - 2){

            S32 groundHeight{heightMap[treeX + treeZ * chunkSizeX]};
            S32 treeY{groundHeight - job.cy * static_cast<S32>(chunkSizeY)};

            if (treeY >= 0 && treeY < static_cast<S32>(chunkSizeY)) {
                if (blocks[VoxelIndex(treeX, treeY - 1, treeZ)] == Voxel::Grass) {
                    U32 treeHeight{4u + (seed >> 16) % 3u};
                    trees.push_back(TreeCandidate{treeX, treeY, treeZ, treeHeight, biome});
                }
            }
        }
    }

    // Finds a cactus candidate
    static void FindCactusCandidate(Vector<CactusCandidate>& cacti, const Vector<S32>& heightMap,
                                   const Vector<Voxel>& blocks, const GenJob& job, U32 seed,
                                   U32 x, U32 z, U32 chunkSizeX, U32 chunkSizeY) {
        S32 offsetX{static_cast<S32>(seed % 3) - 1};
        S32 offsetZ{static_cast<S32>((seed >> 8) % 3) - 1};
        S32 cactusX{static_cast<S32>(x) + offsetX};
        S32 cactusZ{static_cast<S32>(z) + offsetZ};

        if (cactusX >= 1 && cactusX < static_cast<S32>(chunkSizeX) - 1){

            S32 groundHeight{heightMap[cactusX + cactusZ * chunkSizeX]};
            S32 cactusY{groundHeight - job.cy * static_cast<S32>(chunkSizeY)};

            if (cactusY >= 0 && cactusY < static_cast<S32>(chunkSizeY)) {
                if (blocks[VoxelIndex(cactusX, cactusY - 1, cactusZ)] == Voxel::Sand) {
                    U32 cactusHeight{2u + (seed >> 20) % 3u};
                    cacti.push_back(CactusCandidate{cactusX, cactusY, cactusZ, cactusHeight});
                }
            }
        }
    }

    static void FindVegetationCandidates(Vector<TreeCandidate>& trees, Vector<CactusCandidate>& cacti,
                                        const Vector<S32>& heightMap, const Vector<BiomeType>& biomeMap,
                                        const Vector<Voxel>& blocks, const GenJob& job,
                                        U32 chunkSizeX, U32 chunkSizeY, U32 chunkSizeZ) {
        for (U32 z{4}; z < chunkSizeZ - 4; z += 4) {
            for (U32 x{4}; x < chunkSizeX - 4; x += 4) {
                F32 wx{job.origin.x + static_cast<F32>(x) * job.bs};
                F32 wz{job.origin.z + static_cast<F32>(z) * job.bs};
                BiomeType biome = biomeMap[x + z * chunkSizeX];
                BiomeData biomeData = biomes::GetBiomeData(biome);

                U32 seed{noise::wang(static_cast<U32>(wx * 73) ^ static_cast<U32>(wz * 97))};
                F32 vegChance{static_cast<F32>(seed % 100) / 100.0f};

                if (biome == BiomeType::Plains && vegChance < biomeData.treeChance) {
                    FindTreeCandidate(trees, heightMap, blocks, job, seed, x, z, chunkSizeX, chunkSizeY, biome);
                } else if (biome == BiomeType::Desert && vegChance < 0.02f) {
                    FindCactusCandidate(cacti, heightMap, blocks, job, seed, x, z, chunkSizeX, chunkSizeY);
                }
            }
        }
    }

    // Checks if a leaf position is valid
    static bool IsValidLeafPosition(S32 lx, S32 lz, S32 dx, S32 dz, S32 radius, S32 ly) {
        constexpr U32 NX{VoxelChunk::SizeX}, NZ{VoxelChunk::SizeZ};

        if (lx < 0 || lx >= static_cast<S32>(NX) || lz < 0 || lz >= static_cast<S32>(NZ)) {
            return false;
        }

        // Skip corners for more natural shape
        if (radius == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) {
            U32 cornerSeed{noise::wang(static_cast<U32>(lx * 31 + ly * 37 + lz * 41))};
            return (cornerSeed % 100) <= 40; // 40% chance to place corners
        }

        return true;
    }

    // Places the leaves of a tree
    static void PlaceTreeLeaves(Vector<Voxel>& blocks, const TreeCandidate& tree, U32 chunkSizeY) {
        S32 leavesStart{tree.y + static_cast<S32>(tree.height) - 3};
        S32 leavesEnd{tree.y + static_cast<S32>(tree.height) + 1};

        for (S32 ly{leavesStart}; ly <= leavesEnd; ++ly) {
            if (ly < 0 || ly >= static_cast<S32>(chunkSizeY)) continue;

            S32 radius{(ly == leavesEnd || ly == leavesEnd - 1) ? 1 : 2};

            for (S32 dx{-radius}; dx <= radius; ++dx) {
                for (S32 dz{-radius}; dz <= radius; ++dz) {
                    S32 lx{tree.x + dx};
                    S32 lz{tree.z + dz};

                    if (IsValidLeafPosition(lx, lz, dx, dz, radius, ly)) {
                        U32 idx = static_cast<U32>(VoxelIndex(lx, ly, lz));
                        if (blocks[idx] == Voxel::Air) {
                            blocks[idx] = Voxel::Leaves;
                        }
                    }
                }
            }
        }
    }

    // Places a tree
    static void PlaceTree(Vector<Voxel>& blocks, const TreeCandidate& tree, U32 chunkSizeY) {
        // Place trunk
        for (U32 h{}; h < tree.height; ++h) {
            S32 ty{tree.y + static_cast<S32>(h)};
            if (ty >= 0 && ty < static_cast<S32>(chunkSizeY)) {
                blocks[VoxelIndex(tree.x, ty, tree.z)] = Voxel::Log;
            }
        }

        PlaceTreeLeaves(blocks, tree, chunkSizeY);
    }

    // Places a cactus
    static void PlaceCactus(Vector<Voxel>& blocks, const CactusCandidate& cactus, U32 chunkSizeY) {
        for (U32 h{}; h < cactus.height; ++h) {
            S32 cy{cactus.y + static_cast<S32>(h)};
            if (cy >= 0 && cy < static_cast<S32>(chunkSizeY)) {
                blocks[VoxelIndex(cactus.x, cy, cactus.z)] = Voxel::Leaves; // Green to simulate cactus
            }
        }
    }

    // Places all vegetation
    static void PlaceAllVegetation(Vector<Voxel>& blocks,
                                  const Vector<TreeCandidate>& trees,
                                  const Vector<CactusCandidate>& cacti,
                                  U32 chunkSizeY) {
        for (const auto& tree : trees) {
            PlaceTree(blocks, tree, chunkSizeY);
        }

        for (const auto& cactus : cacti) {
            PlaceCactus(blocks, cactus, chunkSizeY);
        }
    }
}

// ===== MAIN SYSTEM =====
export class VoxelGenerationSystem : public System<VoxelGenerationSystem> {

    struct GenResult {
        EntityHandle h;
        Vector<Voxel> blocks;
    };

    static inline Vector<std::thread> s_Workers{};
    static inline std::mutex s_Mutex{};
    static inline std::condition_variable s_CV{};
    static inline std::deque<GenJob> s_Jobs{};
    static inline std::mutex s_ReadyMutex{};
    static inline std::deque<GenResult> s_Ready{};
    static inline std::atomic<bool> s_Stop{false};
    static inline std::once_flag s_Once{};

    // Generates a full chunk
    static void GenerateChunk(const GenJob& job) {
        constexpr U32 NX{VoxelChunk::SizeX}, NY{VoxelChunk::SizeY}, NZ{VoxelChunk::SizeZ};
        Vector<Voxel> blocks{};
        blocks.resize(static_cast<USize>(NX) * NY * NZ);

        // Step 1: Generate heightmap and biomes
        Vector<S32> heightMap{};
        Vector<BiomeType> biomeMap{};
        heightMap.resize(NX * NZ);
        biomeMap.resize(NX * NZ);

        terrain::GenerateHeightMap(heightMap, biomeMap, job, NX, NZ);

        // Step 2: Place the terrain blocks
        terrain::PlaceTerrainBlocks(blocks, heightMap, biomeMap, job, NX, NY, NZ);

        // Step 3: Find vegetation candidates
        Vector<TreeCandidate> trees{};
        Vector<CactusCandidate> cacti{};
        vegetation::FindVegetationCandidates(trees, cacti, heightMap, biomeMap, blocks, job, NX, NY, NZ);

        // Step 4: Place the vegetation
        vegetation::PlaceAllVegetation(blocks, trees, cacti, NY);

        // Step 5: Send the result
        {
            std::lock_guard lk{s_ReadyMutex};
            s_Ready.push_back(GenResult{job.h, std::move(blocks)});
        }
    }

    static void StartPool(U32 threads) {
        std::call_once(s_Once, [threads]() {
            U32 n{threads ? threads : std::max(2u, std::thread::hardware_concurrency())};
            for (U32 i{}; i < n; ++i) {
                s_Workers.emplace_back([]() {
                    for (;;) {
                        GenJob job{};
                        {
                            std::unique_lock lk{s_Mutex};
                            s_CV.wait(lk, []{ return s_Stop.load() || !s_Jobs.empty(); });
                            if (s_Stop.load()) return;
                            job = s_Jobs.front();
                            s_Jobs.pop_front();
                        }

                        GenerateChunk(job);
                    }
                });
            }
        });
    }

public:
    void Setup() {
        SetName("VoxelGeneration");
        SetStage(SystemStage::Update);
        SetPriority(SystemPriority::High);
        SetParallel(true);
        RunBefore("VoxelMeshing");
        StartPool(0);
    }

    ~VoxelGenerationSystem() {
        s_Stop.store(true);
        s_CV.notify_all();
        for (auto &t : s_Workers) if (t.joinable()) t.join();
    }

    void Run(World* world, F32) override {
        auto* wcfgStore{world->GetStorage<VoxelWorldConfig>()};
        assert(wcfgStore && wcfgStore->Size() > 0, "Missing VoxelWorldConfig");
        VoxelWorldConfig const* cfg{};
        for (auto [h,c] : *wcfgStore) { cfg = &c; break; }

        auto* scStore{world->GetStorage<VoxelStreamingConfig>()};
        assert(scStore && scStore->Size() > 0, "Missing VoxelStreamingConfig");
        VoxelStreamingConfig const* sc{};
        for (auto [h,c] : *scStore) { sc = &c; break; }

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
            if (!c.blocks.empty() || c.generating) continue;
            Math::Vec3 center{c.origin.x + 0.5f * sx, c.origin.y + 0.5f * sy, c.origin.z + 0.5f * sz};
            F32 d2{(center - camPos).LengthSquared()};
            todo.push_back(Item{d2, h});
        }

        if (!todo.empty()) std::ranges::sort(todo, {}, &Item::d2);

        U32 enqueueLeft{sc->generateBudget};
        for (auto const& it : todo) {
            if (enqueueLeft == 0u) break;
            auto* chunk{world->GetComponent<VoxelChunk>(it.h)};
            if (!chunk || !chunk->blocks.empty() || chunk->generating) continue;
            chunk->generating = true;

            GenJob job{};
            job.h = it.h;
            job.cx = static_cast<S32>(chunk->cx);
            job.cy = static_cast<S32>(chunk->cy);
            job.cz = static_cast<S32>(chunk->cz);
            job.origin = chunk->origin;
            job.bs = cfg->blockSize;

            {
                std::lock_guard lk{s_Mutex};
                s_Jobs.push_back(job);
            }
            s_CV.notify_one();
            --enqueueLeft;
        }

        U32 applyLeft{sc->generateBudget};
        {
            std::lock_guard lk{s_ReadyMutex};
            while (applyLeft > 0u && !s_Ready.empty()) {
                auto res{std::move(s_Ready.front())};
                s_Ready.pop_front();
                auto* chunk{world->GetComponent<VoxelChunk>(res.h)};
                if (chunk) {
                    chunk->blocks = std::move(res.blocks);
                    chunk->dirty = true;
                    chunk->generating = false;

                    // Mark neighbor chunks as dirty
                    if (auto* store{world->GetStorage<VoxelChunk>()}) {
                        for (auto [hh, cc] : *store) {
                            if (cc.cx + 1 == chunk->cx && cc.cy == chunk->cy && cc.cz == chunk->cz)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                            if (cc.cx == chunk->cx + 1 && cc.cy == chunk->cy && cc.cz == chunk->cz)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                            if (cc.cy + 1 == chunk->cy && cc.cx == chunk->cx && cc.cz == chunk->cz)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                            if (cc.cy == chunk->cy + 1 && cc.cx == chunk->cx && cc.cz == chunk->cz)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                            if (cc.cz + 1 == chunk->cz && cc.cx == chunk->cx && cc.cy == chunk->cy)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                            if (cc.cz == chunk->cz + 1 && cc.cx == chunk->cx && cc.cy == chunk->cy)
                                const_cast<VoxelChunk&>(cc).dirty = true;
                        }
                    }
                }
                --applyLeft;
            }
        }
    }
};