export module Components.ComponentRegistry;

import Core.Types;
import ECS.Component;

export enum ComponentIDs : ComponentID {
    // Core components
    Transform_ID,

    // Rendering components
    Camera_ID,

    // Game specific components
    GAME_COMPONENT_START
};