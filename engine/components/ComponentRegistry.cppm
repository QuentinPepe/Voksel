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
    VoxelWorldConfig_ID,
    VoxelChunk_ID,
    VoxelMesh_ID,
    VoxelRenderResources_ID,
    VoxelStreamingConfig_ID,

    // Game specific components
    GAME_COMPONENT_START
};
