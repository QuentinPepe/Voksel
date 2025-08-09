export module Components.CameraController;

import Core.Types;
import Components.ComponentRegistry;
import ECS.Component;
import Math.Core;
import Math.Vector;
import std;

export struct CameraController {
    enum class Type : U8 {
        Free,
        FPS,
        Orbit,
        Fixed
    };

    Type type{Type::Free};
    F32 moveSpeed{10.0f};
    F32 lookSpeed{0.002f};
    F32 boostMultiplier{3.0f};

    Math::Vec3 velocity{Math::Vec3::Zero};
    F32 yaw{0.0f};
    F32 pitch{0.0f};

    bool enableInput{true};
    bool constrainPitch{true};
    F32 minPitch{-Math::HALF_PI + 0.1f};
    F32 maxPitch{Math::HALF_PI - 0.1f};
};

template<>
struct ComponentTypeID<CameraController> {
    static consteval ComponentID value() {
        return CameraController_ID;
    }
};