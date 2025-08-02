module;
#include <immintrin.h>

export module Systems.MeshGenerator;

import Voxel.Types;
import Voxel.Chunk;
import Core.Types;
import Math.Vector;
import Graphics;
import std;

export namespace Voxel {
    struct VoxelVertex {
        Math::Vec3 position;
        Math::Vec3 normal;
        Math::Vec2 uv;
        U32 color;
    };

    class MeshGenerator {
    private:
        Vector<VoxelVertex> m_Vertices;
        Vector<U32> m_Indices;

        static constexpr F32 FACE_VERTICES[6][4][3]{
            // Front
            {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
            // Back
            {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
            // Left
            {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
            //Right
            {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
            // Top
            {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
            // Bottom
            {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}
        };

        static constexpr F32 FACE_UVS[4][2] = {
            {0, 0}, {1, 0}, {1, 1}, {0, 1}
        };

        static constexpr U32 FACE_INDICES[6] = {0, 1, 2, 0, 2, 3};

        [[nodiscard]] static U32 GetBlockColor(BlockType type) {
            switch (type) {
                case BlockType::Grass: return 0xFF00AA00;
                case BlockType::Stone: return 0xFF8888888;
                case BlockType::Dirt: return 0xFF664422;
                case BlockType::Sand: return 0xFFDDCC99;
                case BlockType::Water: return 0x8800AAFF;
                case BlockType::Wood: return 0xFF553311;
                case BlockType::Leaves: return 0xAA00FF00;
                case BlockType::Glass: return 0x44FFFFFF;
                case BlockType::Cobblestone: return 0xFF666666;
                case BlockType::Bedrock: return 0xFF222222;
                default: return 0xFFFFFFFF;
            }
        }

        void AddFace(const Math::Vec3 &position, Face face, BlockType blockType) {
            U32 baseIndex = static_cast<U32>(m_Vertices.size());
            U32 color = GetBlockColor(blockType);
            Math::Vec3 normal = FACE_NORMALS[static_cast<U8>(face)];

            for (int i = 0; i < 4; ++i) {
                VoxelVertex vertex;
                vertex.position = position + Math::Vec3{
                                      FACE_VERTICES[static_cast<U8>(face)][i][0],
                                      FACE_VERTICES[static_cast<U8>(face)][i][1],
                                      FACE_VERTICES[static_cast<U8>(face)][i][2],
                                  };
                vertex.normal = normal;
                vertex.uv = Math::Vec2{FACE_UVS[i][0], FACE_UVS[i][1]};
                vertex.color = color;

                m_Vertices.push_back(vertex);
            }

            for (U32 index: FACE_INDICES) {
                m_Indices.push_back(baseIndex + index);
            }
        }

    public:
        void GenerateMesh(const Chunk &chunk, const Chunk *neighbors[6]) {
            m_Vertices.clear();
            m_Indices.clear();

            m_Vertices.reserve(chunk.GetSolidVoxelCount() * 24);
            m_Indices.reserve(chunk.GetSolidVoxelCount() * 36);

            const ChunkPos &chunkPos = chunk.GetPosition();
            Math::Vec3 chunkOffset{
                static_cast<F32>(chunkPos.x * CHUNK_SIZE),
                static_cast<F32>(chunkPos.y * CHUNK_SIZE),
                static_cast<F32>(chunkPos.z * CHUNK_SIZE)
            };

            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int y = 0; y < CHUNK_SIZE; ++y) {
                    for (int x = 0; x < CHUNK_SIZE; ++x) {
                        VoxelData voxel = chunk.GetVoxel(x, y, z);
                        if (voxel.IsAir()) continue;

                        BlockType blockType = static_cast<BlockType>(voxel.type);
                        Math::Vec3 voxelPos = chunkOffset + Math::Vec3{
                                                  static_cast<F32>(x),
                                                  static_cast<F32>(y),
                                                  static_cast<F32>(z)
                                              };

                        for (U8 faceIdx = 0; faceIdx < 6; ++faceIdx) {
                            Face face = static_cast<Face>(faceIdx);
                            S32 nx = x + FACE_OFFSETS[faceIdx][0];
                            S32 ny = y + FACE_OFFSETS[faceIdx][1];
                            S32 nz = z + FACE_OFFSETS[faceIdx][2];

                            VoxelData neighbor;
                            if (nx >= 0 && nx < CHUNK_SIZE &&
                                ny >= 0 && ny < CHUNK_SIZE &&
                                nz >= 0 && nz < CHUNK_SIZE) {
                                neighbor = chunk.GetVoxel(nx, ny, nz);
                            } else {
                                const Chunk* neighborChunk = neighbors[faceIdx];
                                if (neighborChunk) {
                                    U32 localX = (nx + CHUNK_SIZE) % CHUNK_SIZE;
                                    U32 localY = (ny + CHUNK_SIZE) % CHUNK_SIZE;
                                    U32 localZ = (nz + CHUNK_SIZE) % CHUNK_SIZE;
                                    neighbor = neighborChunk->GetVoxel(localX, localY, localZ);
                                }
                            }

                            if (neighbor.IsTransparent() && neighbor.type != voxel.type) {
                                AddFace(voxelPos, face, blockType);
                            }
                        }
                    }
                }
            }
        }

        void GenerateSimpleCube(const Math::Vec3& position, BlockType blockType) {
            m_Vertices.clear();
            m_Indices.clear();

            for (U8 faceIdx = 0; faceIdx < 6; ++faceIdx) {
                AddFace(position, static_cast<Face>(faceIdx), blockType);
            }
        }

        [[nodiscard]] const Vector<VoxelVertex>& GetVertices() const { return m_Vertices; }
        [[nodiscard]] const Vector<U32>& GetIndices() const { return m_Indices; }
        [[nodiscard]] bool IsEmpty() const { return m_Vertices.empty(); }
    };
}
