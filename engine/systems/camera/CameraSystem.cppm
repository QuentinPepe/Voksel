export module Systems.CameraSystem;

import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Camera;
import Components.Transform;
import Systems.CameraManager;
import Math.Matrix;
import Core.Types;
import Core.Log;
import std;

export class CameraSystem : public QuerySystem<CameraSystem, Write<Camera>, Read<Transform>> {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("Camera");
        SetStage(SystemStage::PreRender);
        SetPriority(SystemPriority::High);
    }

    void Run(World* world, F32) override {
        ForEach(world, [](Camera* camera, const Transform* transform) {
            camera->view = Math::Mat4::LookAt(
                transform->position,
                transform->position + transform->Forward(),
                transform->Up()
            );
            camera->UpdateViewProjection();
        });
    }
};

export class CameraLifecycleSystem : public System<CameraLifecycleSystem> {
private:
    World* m_World{nullptr};

public:
    void Setup() {
        SetName("CameraLifecycle");
        SetStage(SystemStage::PreUpdate);
        SetPriority(SystemPriority::Critical);
    }

    void Configure(SystemScheduler& scheduler) override {
        System::Configure(scheduler);
    }

    void Run(World* world, F32 dt) override {
        if (!m_World) {
            m_World = world;
            CameraManager::Clear();
        }

        auto* storage = world->GetStorage<Camera>();
        if (!storage) return;

        for (auto [handle, camera] : *storage) {
            auto& cameras = CameraManager::GetAllCameras();
            if (std::ranges::find(cameras, handle) == cameras.end()) {
                CameraManager::RegisterCamera(handle);

                if (camera.isPrimary) {
                    CameraManager::SetPrimaryCamera(handle);
                }
            }
        }
    }
};