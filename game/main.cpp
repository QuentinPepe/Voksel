#include <fstream>

import Core.Types;
import Core.Log;
import Graphics;
import Graphics.Window;
import Graphics.RenderData;
import Graphics.DX12.GraphicsContext;
import Input.Core;
import Input.Manager;
import Input.Window;
import ECS.World;
import ECS.SystemScheduler;
import Components.Transform;
import Components.Camera;
import Components.CameraController;
import Systems.CameraSystem;
import Systems.CameraManager;
import Systems.CameraControllerSystem;
import Components.ComponentRegistry;
import Components.Voxel;
import Systems.VoxelBootstrap;
import Systems.VoxelMeshing;
import Systems.VoxelUpload;
import Systems.VoxelRenderer;
import Math.Core;
import Math.Vector;

int main() {
    Logger::EnableColor(false);
    Logger::Info("Starting Voksel Engine – Voxel demo v{}.{}.{}", 0, 1, 0);

    WindowConfig wc{}; wc.width = 1280; wc.height = 720; wc.title = "Voksel – Voxels"; wc.fullscreen = false; Window window{wc};

    GraphicsConfig gc{}; gc.enableValidation = true; gc.enableVSync = true; auto graphics{CreateGraphicsContext(window, gc)};

    InputManager inputManager{}; WindowInputHandler windowInput{&window, &inputManager};
    windowInput.SetResizeCallback([&graphics](U32 w, U32 h) { if (w > 0 && h > 0) { graphics->OnResize(w, h); } });

    World world{};

    auto cameraEntity{world.CreateEntity()};
    world.AddComponent(cameraEntity, Transform{Math::Vec3{0.0f, 20.0f, 40.0f}});
    world.AddComponent(cameraEntity, Camera{});
    world.AddComponent(cameraEntity, CameraController{});

    auto& ct{*world.GetComponent<Transform>(cameraEntity)}; ct.position = Math::Vec3{0.0f, 20.0f, 40.0f}; ct.LookAt(Math::Vec3::Zero, Math::Vec3::Up);
    auto& cc{*world.GetComponent<Camera>(cameraEntity)}; cc.fov = Math::ToRadians(60.0f); cc.aspectRatio = static_cast<F32>(wc.width) / wc.height; cc.nearPlane = 0.1f; cc.farPlane = 500.0f; cc.UpdateProjection();

    auto cfgEntity{world.CreateEntity()};
    VoxelWorldConfig vcfg{}; vcfg.chunksX = 2; vcfg.chunksY = 1; vcfg.chunksZ = 2; vcfg.blockSize = 1.0f; world.AddComponent(cfgEntity, vcfg);

    SystemScheduler scheduler{};

    auto* cameraSystem{scheduler.AddSystem<CameraSystem>()};
    auto* cameraLifecycle{scheduler.AddSystem<CameraLifecycleSystem>()};
    auto* cameraController{scheduler.AddSystem<CameraControllerSystem>()}; cameraController->SetInputManager(&inputManager);

    auto* voxelBootstrap{scheduler.AddSystem<VoxelBootstrapSystem>()};
    auto* voxelMesher{scheduler.AddSystem<VoxelMeshingSystem>()};
    auto* voxelUpload{scheduler.AddSystem<VoxelUploadSystem>()};
    auto* voxelRenderer{scheduler.AddSystem<VoxelRendererSystem>()};

    voxelUpload->SetGraphicsContext(graphics.get());
    voxelRenderer->SetGraphicsContext(graphics.get());

    scheduler.BuildExecutionGraph(&world);

    windowInput.SetCursorLocked(false);
    windowInput.SetCursorVisible(true);

    Logger::Info("Starting render loop – ESC quits");

    bool shouldExit{false};
    while (!graphics->ShouldClose() && !shouldExit) {
        window.PollEvents();
        if (inputManager.IsKeyJustPressed(Key::Escape)) { shouldExit = true; }

        graphics->BeginFrame();

        const F32 dt{1.0f / 60.0f};
        for (auto&& [stage, nodes] : scheduler.GetStageNodes()) {
            if (stage == SystemStage::PreUpdate || stage == SystemStage::Update || stage == SystemStage::PostUpdate) {
                for (auto* node : nodes) { scheduler.ExecuteSystem(node, dt); }
            }
        }

        for (auto&& [stage, nodes] : scheduler.GetStageNodes()) {
            if (stage == SystemStage::PreRender) {
                for (auto* node : nodes) { scheduler.ExecuteSystem(node, dt); }
            }
        }

        RenderPassInfo pass{}; pass.name = "Voxel Main"; pass.clearColor = true; pass.clearDepth = true; graphics->BeginRenderPass(pass);

        for (auto&& [stage, nodes] : scheduler.GetStageNodes()) {
            if (stage == SystemStage::Render) {
                for (auto* node : nodes) { scheduler.ExecuteSystem(node, dt); }
            }
        }

        graphics->EndRenderPass();

        for (auto&& [stage, nodes] : scheduler.GetStageNodes()) {
            if (stage == SystemStage::PostRender) {
                for (auto* node : nodes) { scheduler.ExecuteSystem(node, dt); }
            }
        }

        graphics->EndFrame();
    }

    Logger::Info("Application closing...");
    return 0;
}
