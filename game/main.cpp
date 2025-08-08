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

import Tasks.Orchestrator;
import Tasks.ECSIntegration;
import Tasks.TaskGraph;

int main() {
    Logger::EnableColor(false);
    Logger::Info("Starting Voksel Engine – Voxel demo v{}.{}.{}", 0, 1, 0);

    WindowConfig wc{};
    wc.width = 1280;
    wc.height = 720;
    wc.title = "Voksel – Voxels";
    wc.fullscreen = false;

    Window window{wc};

    GraphicsConfig gc{};
    gc.enableValidation = true;
    gc.enableVSync = true;

    auto graphics = CreateGraphicsContext(window, gc);

    InputManager inputManager{};
    WindowInputHandler windowInput{&window, &inputManager};

    windowInput.SetResizeCallback([&graphics](U32 w, U32 h) {
        if (w > 0 && h > 0) { graphics->OnResize(w, h); }
    });

    windowInput.SetCursorLocked(false);
    windowInput.SetCursorVisible(true);

    World world{};

    auto cameraEntity = world.CreateEntity();
    world.AddComponent(cameraEntity, Transform{Math::Vec3{0.0f, 20.0f, 40.0f}});
    world.AddComponent(cameraEntity, Camera{});
    world.AddComponent(cameraEntity, CameraController{});

    auto& ct = *world.GetComponent<Transform>(cameraEntity);
    ct.position = Math::Vec3{0.0f, 20.0f, 40.0f};
    ct.LookAt(Math::Vec3::Zero, Math::Vec3::Up);

    auto& cc = *world.GetComponent<Camera>(cameraEntity);
    cc.fov = Math::ToRadians(60.0f);
    cc.aspectRatio = static_cast<F32>(wc.width) / wc.height;
    cc.nearPlane = 0.1f;
    cc.farPlane = 500.0f;
    cc.UpdateProjection();

    auto cfgEntity = world.CreateEntity();
    VoxelWorldConfig vcfg{};
    vcfg.chunksX = 2; vcfg.chunksY = 1; vcfg.chunksZ = 2; vcfg.blockSize = 1.0f;
    world.AddComponent(cfgEntity, vcfg);

    EngineOrchestrator orchestrator{0};
    orchestrator.SetInputManager(&inputManager);
    orchestrator.SetWindow(&window);
    orchestrator.SetWorld(&world);
    orchestrator.SetGraphicsContext(graphics.get());

    EngineOrchestratorECS orchestratorECS{&orchestrator};
    SystemScheduler* scheduler = orchestratorECS.GetSystemScheduler();

    auto* cameraSystem       = scheduler->AddSystem<CameraSystem>();
    auto* cameraLifecycle    = scheduler->AddSystem<CameraLifecycleSystem>();
    auto* cameraController   = scheduler->AddSystem<CameraControllerSystem>();
    cameraController->SetInputManager(&inputManager);
    cameraController->SetWindowInputHandler(&windowInput);

    auto* voxelBootstrap     = scheduler->AddSystem<VoxelBootstrapSystem>();
    auto* voxelMesher        = scheduler->AddSystem<VoxelMeshingSystem>();
    auto* voxelUpload        = scheduler->AddSystem<VoxelUploadSystem>();
    auto* voxelRenderer      = scheduler->AddSystem<VoxelRendererSystem>();

    voxelUpload->SetGraphicsContext(graphics.get());
    voxelRenderer->SetGraphicsContext(graphics.get());

    orchestratorECS.BuildECSExecutionGraph(&world);

    orchestrator.SetPreFrameCallback([&](EngineOrchestrator::FrameData& frame) {
        orchestratorECS.UpdateECS(static_cast<F32>(frame.deltaTime));
    });

    orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData&) {
        if (inputManager.IsKeyJustPressed(Key::Escape)) {
            window.RequestClose();
        }
    });

    orchestrator.SetUpdateCallback([&](EngineOrchestrator::FrameData&) {

    });

    {
        using P = EngineOrchestrator::PhaseNames;

        RenderPassInfo voxelPass{};
        voxelPass.name = "Voxel Main";
        voxelPass.clearColor = true;
        voxelPass.clearDepth = true;

        IGraphicsContext* gfx = graphics.get();

        orchestrator.AddTaskToPhase(
            P::Render,
            "BeginVoxelPass",
            [gfx, voxelPass]() mutable {
                gfx->BeginRenderPass(voxelPass);
            },
            TaskPriority::High
        );

        orchestrator.AddTaskToPhase(
            P::Render,
            "EndVoxelPass",
            [gfx]() {
                gfx->EndRenderPass();
            },
            TaskPriority::High
        );

        for (auto&& [stage, nodes] : scheduler->GetStageNodes()) {
            if (stage == SystemStage::Render) {
                for (auto* node : nodes) {
                    const std::string& sysName = node->metadata.name;
                    orchestrator.AddTaskDependency(P::Render, sysName, "BeginVoxelPass");
                    orchestrator.AddTaskDependency(P::Render, "EndVoxelPass", sysName);
                }
            }
        }
    }

    Logger::Info("Starting render loop – ESC quits");

    while (!graphics->ShouldClose()) {
        windowInput.Flush();
        orchestrator.ExecuteFrame();
    }

    Logger::Info("Application closing...");
    return 0;
}
