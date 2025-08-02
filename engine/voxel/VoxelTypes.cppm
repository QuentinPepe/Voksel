export module Voxel.Types;

import Core.Types;
import Math.Vector;
import std;

export namespace Voxel {
    using BlockID = U16;

    inline constexpr U32 CHUNK_SIZE = 32;
    inline constexpr U32 CHUNK_SIZE_SQUARED = CHUNK_SIZE * CHUNK_SIZE;
    inline constexpr U32 CHUNK_SIZE_CUBED = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    inline constexpr U32 CHUNK_SIZE_MASK = CHUNK_SIZE - 1;
    inline constexpr U32 CHUNK_SIZE_SHIFT = 5;

    enum class BlockType : BlockID {
        Air = 0,
        Stone = 1,
        Dirt = 2,
        Grass = 3,
        Sand = 4,
        Water = 5,
        Wood = 6,
        Leaves = 7,
        Glass = 8,
        Cobblestone = 9,
        Bedrock = 10
    };

    struct alignas(4) VoxelData {
        BlockID type: 16;
        U8 slyLight: 4;
        U8 blockLight: 4;
        U8 metadata: 8;

        constexpr VoxelData() : type{0}, slyLight{0}, blockLight{0}, metadata{0} {
        }

        constexpr VoxelData(BlockID type) : type{type}, slyLight{15}, blockLight{0}, metadata{0} {
        }

        [[nodiscard]] constexpr bool IsAir() const { return type == 0; }
        [[nodiscard]] constexpr bool IsSolid() const { return type != 0; }

        [[nodiscard]] constexpr bool IsTransparent() const {
            return type == 0 ||
                   type == static_cast<BlockID>(BlockType::Water) ||
                   type == static_cast<BlockID>(BlockType::Glass) ||
                   type == static_cast<BlockID>(BlockType::Leaves);
        }
    };

    static_assert(sizeof(VoxelData) == 4);

    struct ChunkPos {
        S32 x, y, z;

        constexpr ChunkPos() : x{0}, y{0}, z{0} {
        }

        constexpr ChunkPos(S32 x, S32 y, S32 z) : x{x}, y{y}, z{z} {
        }

        [[nodiscard]] constexpr bool operator==(const ChunkPos &) const = default;

        [[nodiscard]] constexpr U64 Hash() const {
            return (static_cast<U64>(x) & 0xFFFF) |
                   ((static_cast<U64>(y) & 0xFFFF) << 16) |
                   ((static_cast<U64>(z) & 0xFFFF) << 32);
        }
    };

    struct BlockPos {
        S32 x, y, z;

        [[nodiscard]] constexpr ChunkPos GetChunkPos() const {
            return ChunkPos{
                x >> CHUNK_SIZE_SHIFT,
                y >> CHUNK_SIZE_SHIFT,
                z >> CHUNK_SIZE_SHIFT
            };
        }

        [[nodiscard]] constexpr U32 GetLocalIndex() const {
            const U32 lx = x & CHUNK_SIZE_MASK;
            const U32 ly = y & CHUNK_SIZE_MASK;
            const U32 lz = z & CHUNK_SIZE_MASK;
            return lx + ly * CHUNK_SIZE + lz * CHUNK_SIZE_SQUARED;
        }
    };

    enum class Face : U8 {
        Front = 0,
        Back = 1,
        Left = 2,
        Right = 3,
        Top = 4,
        Bottom = 5
    };

    inline constexpr Math::Vec3 FACE_NORMALS[6] = {
        {0.0f, 0.0f, 1.0f}, // Front
        {0.0f, 0.0f, -1.0f}, // Back
        {-1.0f, 0.0f, 0.0f}, // Left
        {1.0f, 0.0f, 0.0f}, // Right
        {0.0f, 1.0f, 0.0f}, // Top
        {0.0f, -1.0f, 0.0f}, // Bottom
    };

    inline constexpr S32 FACE_OFFSETS[6][3]{
        {0, 0, 1}, // Front
        {0, 0, -1}, // Back
        {-1, 0, 0}, // Left
        {1, 0, 0}, // Right
        {0, 1, 0}, // Top
        {0, -1, 0}, // Bottom
    };
}

export template<>
struct std::hash<Voxel::ChunkPos> {
    [[nodiscard]] constexpr size_t operator()(const Voxel::ChunkPos& pos) const noexcept {
        return pos.Hash();
    }
};
