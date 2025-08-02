export module Components.ComponentRegistry;

import Core.Types;
import ECS.Component;

export enum ComponentIDs : ComponentID {
    // Core components
    Transform_ID,

    // Rendering components
    Camera_ID,
    CameraController_ID,

    // Voxel
    ChunkComponent_ID,
    ChunkMesh_ID,
    ChunkNeighbors_ID,
    VoxelWorld_ID,

    // Game specific components
    GAME_COMPONENT_START
};