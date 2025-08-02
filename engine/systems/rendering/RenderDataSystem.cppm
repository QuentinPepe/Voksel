export module Systems.RenderSystem;

import ECS.Component;
import ECS.SystemScheduler;
import ECS.Query;
import ECS.World;
import Components.Camera;
import Components.Transform;
import Systems.CameraManager;
import Graphics;
import Graphics.RenderData;
import Core.Types;
import Core.Log;
import std;

export class RenderDataSystem : public System<RenderDataSystem> {
private:
    IGraphicsContext* m_Graphics{nullptr};
    U32 m_CameraConstantBuffer{INVALID_INDEX};
    CameraConstants m_CameraData{};

public:
    void Setup() {
        SetName("RenderData");
        SetStage(SystemStage::PreRender);
        SetPriority(SystemPriority::Critical);
        RunAfter("Camera");
    }

    void SetGraphicsContext(IGraphicsContext* graphics) {
        m_Graphics = graphics;
        if (m_Graphics && m_CameraConstantBuffer == INVALID_INDEX) {
            // Create constant buffer for camera data
            m_CameraConstantBuffer = m_Graphics->CreateConstantBuffer(sizeof(CameraConstants));
        }
    }

    void Run(World* world, F32 /*dt*/) override {
        if (!m_Graphics) return;

        // Get primary camera
        EntityHandle primaryCamera = CameraManager::GetPrimaryCamera();
        if (!primaryCamera.valid()) return;

        // Get camera components
        const Camera* camera = world->GetComponent<Camera>(primaryCamera);
        const Transform* transform = world->GetComponent<Transform>(primaryCamera);

        if (!camera || !transform) return;

        // Update camera data
        m_CameraData.view = camera->view;
        m_CameraData.projection = camera->projection;
        m_CameraData.viewProjection = camera->viewProjection;
        m_CameraData.cameraPosition = transform->position;

        // Upload to GPU
        m_Graphics->UpdateConstantBuffer(m_CameraConstantBuffer, &m_CameraData, sizeof(CameraConstants));
    }

    [[nodiscard]] U32 GetCameraConstantBuffer() const { return m_CameraConstantBuffer; }
};