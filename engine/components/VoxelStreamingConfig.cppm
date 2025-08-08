export module Components.VoxelStreaming;

import Core.Types;
import ECS.Component;
import Components.ComponentRegistry;

export struct VoxelStreamingConfig {
    U32 radius{8};
    U32 margin{2};
    S32 minChunkY{-1};
    S32 maxChunkY{1};
    U32 createBudget{16};
    U32 removeBudget{16};
    U32 generateBudget{4};
    U32 meshBudget{4};
    U32 uploadBudget{4};
};

export template<>
struct ComponentTypeID<VoxelStreamingConfig> {
    static consteval ComponentID value() { return VoxelStreamingConfig_ID; }
};
