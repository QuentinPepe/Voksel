export module Components.Transform;

import Core.Types;
import Components.ComponentRegistry;
import ECS.Component;
import Math.Core;
import Math.Vector;
import Math.Quaternion;
import Math.Matrix;
import std;

export struct Transform {
    Math::Vec3 position{Math::Vec3::Zero};
    Math::Quat rotation{Math::Quat::Identity};
    Math::Vec3 scale{Math::Vec3::One};

    // Local space data
    Math::Mat4 localMatrix{Math::Mat4::Identity};
    bool localDirty{true};

    // World space data
    Math::Mat4 worldMatrix{Math::Mat4::Identity};
    bool worldDirty{true};

    constexpr Transform() = default;

    constexpr Transform(const Math::Vec3 &pos) : position{pos} {
    }

    constexpr Transform(const Math::Vec3 &pos, const Math::Quat &rot)
        : position{pos}, rotation{rot} {
    }

    constexpr Transform(const Math::Vec3 &pos, const Math::Quat &rot, const Math::Vec3 &scl)
        : position{pos}, rotation{rot}, scale{scl} {
    }

    [[nodiscard]] Math::Mat4 GetLocalMatrix() const {
        if (localDirty) {
            UpdateLocalMatrix();
        }
        return localMatrix;
    }

    [[nodiscard]] Math::Vec3 TransformPoint(const Math::Vec3 &point) const {
        return position + rotation * (scale * point);
    }

    [[nodiscard]] Math::Vec3 TransformVector(const Math::Vec3 &vector) const {
        return rotation * (scale * vector);
    }

    [[nodiscard]] Math::Vec3 TransformDirection(const Math::Vec3 &direction) const {
        return rotation * direction;
    }

    [[nodiscard]] Math::Vec3 Forward() const { return rotation * Math::Vec3::Forward; }
    [[nodiscard]] Math::Vec3 Right() const { return rotation * Math::Vec3::Right; }
    [[nodiscard]] Math::Vec3 Up() const { return rotation * Math::Vec3::Up; }

    void LookAt(const Math::Vec3 &target, const Math::Vec3 &up = Math::Vec3::Up) {
        Math::Vec3 forward = (target - position).Normalized();
        rotation = Math::Quat::LookRotation(forward, up);
        localDirty = true;
        worldDirty = true;
    }

    void SetPosition(const Math::Vec3 &pos) {
        position = pos;
        localDirty = true;
        worldDirty = true;
    }

    void SetRotation(const Math::Quat &rot) {
        rotation = rot;
        localDirty = true;
        worldDirty = true;
    }

    void SetScale(const Math::Vec3 &scl) {
        scale = scl;
        localDirty = true;
        worldDirty = true;
    }

private:
    void UpdateLocalMatrix() const {
        Math::Mat4 t = Math::Mat4::Translation(position);
        Math::Mat4 r = rotation.ToMatrix4();
        Math::Mat4 s = Math::Mat4::Scale(scale);
        const_cast<Transform *>(this)->localMatrix = t * r * s;
        const_cast<Transform *>(this)->localDirty = false;
    }
};

template<>
struct ComponentTypeID<Transform> {
    static consteval ComponentID value() {
        return Transform_ID;
    }
};
