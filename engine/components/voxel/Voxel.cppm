export module Components.Voxel;

import Core.Types;
import Core.Assert;
import ECS.Component;
import Components.ComponentRegistry;
import Math.Vector;
import Graphics;
import std;

export enum class Voxel : U8 {
    Air = 0,
    Dirt = 1,
    Grass = 2,
    Stone = 3,
    Log = 4,
    Leaves = 5,
    Sand = 6,
    Water = 7
};

export struct VoxelWorldConfig {
    U32 chunksX{1};
    U32 chunksY{1};
    U32 chunksZ{1};
    F32 blockSize{1.0f};
};

export struct VoxelChunk {
    static constexpr U32 SizeX{32};
    static constexpr U32 SizeY{32};
    static constexpr U32 SizeZ{32};
    U32 cx{0};
    U32 cy{0};
    U32 cz{0};
    Math::Vec3 origin{0.0f, 0.0f, 0.0f};
    Vector<Voxel> blocks{};
    bool dirty{true};
    bool generating{false};
};

export struct VoxelMesh {
    Vector<Vertex> cpuVertices{};
    Vector<U32> cpuIndices{};
    U32 vertexBuffer{INVALID_INDEX};
    U32 indexBuffer{INVALID_INDEX};
    U32 vertexCount{0};
    U32 indexCount{0};
    bool gpuDirty{false};
    bool meshing{false};
    Vector<Vertex> readyVertices{};
};

export struct VoxelRenderResources {
    U32 pipeline{INVALID_INDEX};
};

export template<>
struct ComponentTypeID<VoxelWorldConfig> {
    static consteval ComponentID value() { return VoxelWorldConfig_ID; }
};

export template<>
struct ComponentTypeID<VoxelChunk> {
    static consteval ComponentID value() { return VoxelChunk_ID; }
};

export template<>
struct ComponentTypeID<VoxelMesh> {
    static consteval ComponentID value() { return VoxelMesh_ID; }
};

export template<>
struct ComponentTypeID<VoxelRenderResources> {
    static consteval ComponentID value() { return VoxelRenderResources_ID; }
};

export inline USize VoxelIndex(U32 x, U32 y, U32 z) {
    return static_cast<USize>(x)
        + static_cast<USize>(y) * VoxelChunk::SizeX
        + static_cast<USize>(z) * VoxelChunk::SizeX * VoxelChunk::SizeY;
}

export inline bool InBounds(S32 x, S32 y, S32 z) {
    return x >= 0 && y >= 0 && z >= 0
        && x < static_cast<S32>(VoxelChunk::SizeX)
        && y < static_cast<S32>(VoxelChunk::SizeY)
        && z < static_cast<S32>(VoxelChunk::SizeZ);
}

export inline U32 PackRGBA8(U8 r, U8 g, U8 b, U8 a) {
    return static_cast<U32>(r)
        | (static_cast<U32>(g) << 8)
        | (static_cast<U32>(b) << 16)
        | (static_cast<U32>(a) << 24);
}

export inline U32 ColorForVoxel(Voxel v) {
    switch (v) {
    case Voxel::Dirt:  return PackRGBA8(134, 96,  67,  255);
    case Voxel::Grass: return PackRGBA8(95,  159, 53,  255);
    case Voxel::Stone: return PackRGBA8(130, 130, 130, 255);
    default:           return PackRGBA8(0,   0,   0,   0);
    }
}