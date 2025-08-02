module;
#include <immintrin.h>

export module Voxel.Chunk;

import Voxel.Types;
import Core.Types;
import Core.Assert;
import Math.Vector;
import Math.Transform;
import std;

export namespace Voxel {
    class Chunk {
    private:
        alignas(64) VoxelData m_Voxels[CHUNK_SIZE_CUBED];
        ChunkPos m_Position;
        std::atomic<bool> m_IsDirty{true};
        std::atomic<bool> m_IsEmpty{true};
        std::atomic<U32> m_SolidVoxelCount{0};

    public:
        explicit Chunk(const ChunkPos &pos) : m_Position{pos} {
            std::memset(m_Voxels, 0, sizeof(m_Voxels));
        }

        void SetVoxel(U32 x, U32 y, U32 z, VoxelData voxel) {
            assert(x < CHUNK_SIZE && y < CHUNK_SIZE && z < CHUNK_SIZE, "Voxel position out of bounds");

            U32 index = x + y * CHUNK_SIZE + z * CHUNK_SIZE_SQUARED;
            VoxelData oldVoxel = m_Voxels[index];

            if (oldVoxel.type != voxel.type) {
                m_Voxels[index] = voxel;
                m_IsDirty.store(true);

                if (oldVoxel.IsSolid() && !voxel.IsSolid()) {
                    m_SolidVoxelCount.fetch_sub(1);
                } else if (!oldVoxel.IsSolid() && voxel.IsSolid()) {
                    m_SolidVoxelCount.fetch_add(1);
                }

                m_IsEmpty.store(m_SolidVoxelCount.load() == 0);
            }
        }

        [[nodiscard]] VoxelData GetVoxel(U32 x, U32 y, U32 z) const {
            assert(x < CHUNK_SIZE && y < CHUNK_SIZE && z < CHUNK_SIZE, "Voxel position out of bounds");
            return m_Voxels[x + y * CHUNK_SIZE + z * CHUNK_SIZE_SQUARED];
        }

        [[nodiscard]] VoxelData GetVoxelSafe(S32 x, S32 y, S32 z) const {
            if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
                return VoxelData{};
            }
            return m_Voxels[x + y * CHUNK_SIZE + z * CHUNK_SIZE_SQUARED];
        }

        void Fill(VoxelData voxel) {
            __m128i voxelPacked = _mm_set1_epi32(*reinterpret_cast<const S32 *>(&voxel));

            for (U32 i = 0; i < CHUNK_SIZE_CUBED; i += 4) {
                _mm_store_si128(reinterpret_cast<__m128i *>(&m_Voxels[i]), voxelPacked);
            }

            m_IsDirty.store(true);
            m_SolidVoxelCount.store(voxel.IsSolid() ? CHUNK_SIZE_CUBED : 0);
            m_IsEmpty.store(!voxel.IsSolid());
        }

        void GenerateTerrain(F32 noiseScale = 0.1f) {
            for (U32 z = 0; z < CHUNK_SIZE; ++z) {
                for (U32 x = 0; x < CHUNK_SIZE; ++x) {
                    S32 worldX = m_Position.x * CHUNK_SIZE + x;
                    S32 worldZ = m_Position.z * CHUNK_SIZE + z;

                    F32 height = 10.0f +
                                 std::sin(worldX * noiseScale) * 5.0f +
                                 std::cos(worldZ * noiseScale) * 5.0f;

                    S32 maxY = static_cast<S32>(height);

                    for (U32 y = 0; y < CHUNK_SIZE; ++y) {
                        S32 worldY = m_Position.y * CHUNK_SIZE + y;

                        VoxelData voxel;
                        if (worldY < maxY - 3) {
                            voxel = VoxelData{static_cast<BlockID>(BlockType::Stone)};
                        } else if (worldY < maxY) {
                            voxel = VoxelData{static_cast<BlockID>(BlockType::Dirt)};
                        } else if (worldY == maxY) {
                            voxel = VoxelData{static_cast<BlockID>(BlockType::Grass)};
                        } else {
                            voxel = VoxelData{static_cast<BlockID>(BlockType::Air)};
                        }

                        SetVoxel(x, y, z, voxel);
                    }
                }
            }
        }

        [[nodiscard]] bool IsDirty() const { return m_IsDirty.load(); }
        [[nodiscard]] bool IsEmpty() const { return m_IsEmpty.load(); }
        [[nodiscard]] const ChunkPos& GetPosition() const { return m_Position; }
        [[nodiscard]] U32 GetSolidVoxelCount() const { return m_SolidVoxelCount.load(); }

        void MarkClean() { m_IsDirty.store(false); }

        [[nodiscard]] Math::Bounds GetBounds() const {
            const Math::Vec3 min{
                static_cast<F32>(m_Position.x * CHUNK_SIZE),
                static_cast<F32>(m_Position.y * CHUNK_SIZE),
                static_cast<F32>(m_Position.z * CHUNK_SIZE),
            };
            const Math::Vec3 max = min + Math::Vec3{CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE};
            return Math::Bounds{min, max};
        }
    };
}
