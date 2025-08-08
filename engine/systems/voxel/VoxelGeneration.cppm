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
}

export class VoxelGenerationSystem : public System<VoxelGenerationSystem> {
    struct GenJob {
        EntityHandle h;
        S32 cx;
        S32 cy;
        S32 cz;
        Math::Vec3 origin;
        F32 bs;
    };

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

                        constexpr U32 NX{VoxelChunk::SizeX}, NY{VoxelChunk::SizeY}, NZ{VoxelChunk::SizeZ};
                        Vector<Voxel> blocks{};
                        blocks.resize(static_cast<USize>(NX) * NY * NZ);

                        const F32 freq{0.025f};
                        const F32 hBase{12.0f};
                        const F32 hAmp{10.0f};

                        for (U32 z{}; z < NZ; ++z) {
                            for (U32 x{}; x < NX; ++x) {
                                F32 wx{job.origin.x + static_cast<F32>(x) * job.bs};
                                F32 wz{job.origin.z + static_cast<F32>(z) * job.bs};
                                F32 n{noise::fbm2D(wx * freq, wz * freq, 4u, 0.5f, 2.0f)};
                                S32 h{static_cast<S32>(hBase + n * hAmp)};

                                for (U32 y{}; y < NY; ++y) {
                                    S32 gy{job.cy * static_cast<S32>(NY) + static_cast<S32>(y)};
                                    Voxel v{Voxel::Air};
                                    if (gy < h - 3) v = Voxel::Stone;
                                    else if (gy < h - 1) v = Voxel::Dirt;
                                    else if (gy == h - 1) v = Voxel::Grass;
                                    blocks[VoxelIndex(x, y, z)] = v;
                                }
                            }
                        }

                        {
                            std::lock_guard lk{s_ReadyMutex};
                            s_Ready.push_back(GenResult{job.h, std::move(blocks)});
                        }
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