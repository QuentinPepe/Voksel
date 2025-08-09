export module Components.VoxelSelection;

import Core.Types;
import Math.Vector;
import ECS.Component;
import Components.ComponentRegistry;

export struct VoxelSelection {
    bool valid{false};
    S32 gx{0}, gy{0}, gz{0};
    S32 pgx{0}, pgy{0}, pgz{0};
    Math::Vec3 normal{};
};

export template<>
struct ComponentTypeID<VoxelSelection> {
    static consteval ComponentID value() { return VoxelSelection_ID; }
};