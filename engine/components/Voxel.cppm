export module Components.Voxel;

import Voxel.Types;
import Voxel.Chunk;
import Components.ComponentRegistry;
import ECS.Component;
import Core.Types;
import Graphics;
import std;

export namespace Voxel {
    struct ChunkComponent {
        std::unique_ptr<Chunk> chunk;
        ChunkPos position;
        bool needsRemesh{true};

        ChunkComponent() = default;
        explicit ChunkComponent(const ChunkPos& pos) : chunk{std::make_unique<Chunk>(pos)}, position{pos} {}
    };

    struct ChunkMesh {
        U32 vertexBuffer{INVALID_INDEX};
        U32 indexBuffer{INVALID_INDEX};
        int vertexCount{0};
        bool isValid{false};
    };

    struct ChunkNeighbors {
        EntityHandle neighbors[6];

        void SetNeighbor(Face face, EntityHandle handle) {
            neighbors[static_cast<U8>(face)] = handle;
        }

        [[nodiscard]] EntityHandle GetNeighbor(Face face) const {
            return neighbors[static_cast<U8>(face)];
        }
    };

    struct VoxelWorld {
        UnorderedMap<ChunkPos, EntityHandle> chunkMap;
        S32 viewDistances{8};
        ChunkPos centerChunk;
    };
}

template<>
struct ComponentTypeID<Voxel::ChunkComponent> {
    static consteval ComponentIDs value() {
        return ChunkComponent_ID;
    }
};

template<>
struct ComponentTypeID<Voxel::ChunkMesh> {
    static consteval ComponentIDs value() {
        return ChunkMesh_ID;
    }
};

template<>
struct ComponentTypeID<Voxel::ChunkNeighbors> {
    static consteval ComponentIDs value() {
        return ChunkNeighbors_ID;
    }
};

template<>
struct ComponentTypeID<Voxel::VoxelWorld> {
    static consteval ComponentIDs value() {
        return VoxelWorld_ID;
    }
};