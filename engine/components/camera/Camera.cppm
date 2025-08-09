export module Components.Camera;

import Core.Types;
import Components.ComponentRegistry;
import ECS.Component;
import Math.Core;
import Math.Vector;
import Math.Matrix;
import std;

export struct Camera {
    Math::Mat4 projection{Math::Mat4::Identity};
    Math::Mat4 view{Math::Mat4::Identity};
    Math::Mat4 viewProjection{Math::Mat4::Identity};

    F32 fov{Math::ToRadians(60.0f)};
    F32 aspectRatio{16.0f / 9.0f};
    F32 nearPlane{0.1f};
    F32 farPlane{1000.0f};

    bool isPrimary{false};
    bool needsUpdate{true};

    void UpdateProjection() {
        projection = Math::Mat4::Perspective(fov, aspectRatio, nearPlane, farPlane);
        needsUpdate = true;
    }

    void UpdateViewProjection() {
        viewProjection = projection * view;
        needsUpdate = false;
    }
};

template<>
struct ComponentTypeID<Camera> {
    static consteval ComponentID value() {
        return Camera_ID;
    }
};