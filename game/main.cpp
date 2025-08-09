#include <fstream>

import Core.Types;
import Core.Log;
import Core.Assert;

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
import Components.VoxelStreaming;

import Systems.VoxelStreaming;
import Systems.VoxelGeneration;
import Systems.VoxelMeshing;
import Systems.VoxelUpload;
import Systems.VoxelRenderer;
import Systems.VoxelEdit;
import Systems.VoxelSelectionRender;

import Math.Core;
import Math.Vector;

import Tasks.Orchestrator;
import Tasks.ECSIntegration;
import Tasks.TaskGraph;
import Tasks.TaskProfiler;

import Systems.UI;
import UI.Core;

int main() {
    Logger::EnableColor(false);
    Logger::Info("Starting Voksel Engine – Voxel demo v{}.{}.{}", 0, 2, 0);

    WindowConfig wc{};
    wc.width = 1280;
    wc.height = 720;
    wc.title = "Voksel – Voxels";
    wc.fullscreen = false;

    Window window{wc};

    GraphicsConfig gc{};
    gc.enableValidation = true;
    gc.enableVSync = true;

    auto graphics{CreateGraphicsContext(window, gc)};
    assert(graphics != nullptr, "Graphics creation failed");

    InputManager inputManager{};
    WindowInputHandler windowInput{&window, &inputManager};

    windowInput.SetResizeCallback([&graphics](U32 w, U32 h) {
        if (w > 0 && h > 0) { graphics->OnResize(w, h); }
    });

    windowInput.SetCursorLocked(false);
    windowInput.SetCursorVisible(true);

    World world{};

    auto cameraEntity{world.CreateEntity()};
    world.AddComponent(cameraEntity, Transform{Math::Vec3{0.0f, 20.0f, 40.0f}});
    world.AddComponent(cameraEntity, Camera{});
    world.AddComponent(cameraEntity, CameraController{});

    auto &ct{*world.GetComponent<Transform>(cameraEntity)};
    ct.position = Math::Vec3{0.0f, 20.0f, 40.0f};
    ct.LookAt(Math::Vec3::Zero, Math::Vec3::Up);

    auto &cc{*world.GetComponent<Camera>(cameraEntity)};
    cc.fov = Math::ToRadians(60.0f);
    cc.aspectRatio = static_cast<F32>(wc.width) / wc.height;
    cc.nearPlane = 0.1f;
    cc.farPlane = 500.0f;
    cc.isPrimary = true;
    cc.UpdateProjection();

    auto cfgEntity{world.CreateEntity()};
    VoxelWorldConfig vcfg{};
    vcfg.blockSize = 1.0f;
    world.AddComponent(cfgEntity, vcfg);

    auto streamCfgEntity{world.CreateEntity()};
    VoxelStreamingConfig scfg{};
    scfg.radius = 8;
    scfg.margin = 2;
    scfg.minChunkY = -1;
    scfg.maxChunkY = 1;
    scfg.createBudget = 16;
    scfg.removeBudget = 16;
    world.AddComponent(streamCfgEntity, scfg);

    EngineOrchestrator orchestrator{0};
    orchestrator.SetInputManager(&inputManager);
    orchestrator.SetWindow(&window);
    orchestrator.SetWorld(&world);
    orchestrator.SetGraphicsContext(graphics.get());

    EngineOrchestratorECS orchestratorECS{&orchestrator};
    SystemScheduler *scheduler{orchestratorECS.GetSystemScheduler()};

    auto *cameraSystem{scheduler->AddSystem<CameraSystem>()};
    auto *cameraLifecycle{scheduler->AddSystem<CameraLifecycleSystem>()};
    auto *cameraController{scheduler->AddSystem<CameraControllerSystem>()};
    cameraController->SetInputManager(&inputManager);
    cameraController->SetWindowInputHandler(&windowInput);

    auto *voxelStreamer{scheduler->AddSystem<VoxelStreamingSystem>()};
    auto *voxelGen{scheduler->AddSystem<VoxelGenerationSystem>()};
    auto *voxelMesher{scheduler->AddSystem<VoxelMeshingSystem>()};
    auto *voxelUpload{scheduler->AddSystem<VoxelUploadSystem>()};
    auto *voxelRenderer{scheduler->AddSystem<VoxelRendererSystem>()};
    auto *voxelEdit{scheduler->AddSystem<VoxelEditSystem>()};
    voxelEdit->SetInputManager(&inputManager);
    auto *voxelSel{scheduler->AddSystem<VoxelSelectionRenderSystem>()};

    voxelUpload->SetGraphicsContext(graphics.get());
    voxelRenderer->SetGraphicsContext(graphics.get());
    voxelSel->SetGraphicsContext(graphics.get());

    auto *uiSys{scheduler->AddSystem<UISystem>()};
    uiSys->SetGraphicsContext(graphics.get());
    uiSys->SetWindow(&window);
    uiSys->SetInputManager(&inputManager);
    uiSys->RegisterPanel([](UIContext& ui) {
        UILayout root{};
        root.dir = UIDirection::Column;
        root.width = UIUnit::Percent(0.30f);
        root.height = UIUnit::Auto();
        root.pad = 8.0f;
        root.gap = 6.0f;
        root.anchor = Math::Vec2{0.0f, 0.0f};
        root.offset = Math::Vec2{12.0f, 12.0f};
        auto panel{ui.Begin(root, 0x222222AAu, 6.0f)};

        UILayout row{};
        row.dir = UIDirection::Row;
        row.width = UIUnit::Percent(1.0f);
        row.height = UIUnit::Px(36.0f);
        row.gap = 6.0f;
        auto r{ui.Begin(row)};

        UILayout bL{};
        bL.width = UIUnit::Percent(0.5f);
        bL.height = UIUnit::Percent(1.0f);
        bool a{ui.Button("A", bL, 0x2E7DD1FFu, 4.0f)};

        UILayout bR{};
        bR.width = UIUnit::Percent(0.5f);
        bR.height = UIUnit::Percent(1.0f);
        bool b{ui.Button("B", bR, 0x51C151FFu, 4.0f)};
        ui.End(r);

        UILayout bar{};
        bar.width = UIUnit::Percent(1.0f);
        bar.height = UIUnit::Px(10.0f);
        F32 t{ui.Time01()};
        ui.Progress(bar, t, 0xFFFFFFFFu, 0x2E7DD1FFu, 3.0f);

        ui.End(panel);
    });

    orchestratorECS.BuildECSExecutionGraph(&world);

    orchestrator.SetProfilingEnabled(true);
    TaskProfiler::Get().SetEnabled(true);

    orchestrator.SetPreFrameCallback([&](EngineOrchestrator::FrameData &frame) {
        orchestratorECS.UpdateECS(static_cast<F32>(frame.deltaTime));
    });

    orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData &) {
        if (inputManager.IsKeyJustPressed(Key::Escape)) {
            window.RequestClose();
        }

        if (inputManager.IsKeyJustPressed(Key::F2)) {
            auto report = TaskProfiler::Get().GenerateReport();
            if (!report.empty()) {
                TaskProfiler::Get().SaveToFile("output/profiler_data.txt");
                Logger::Info("Profiler data saved");
            }
        }

        if (inputManager.IsKeyJustPressed(Key::F3)) {
            bool profilingEnabled = !orchestrator.IsProfilingEnabled();
            orchestrator.SetProfilingEnabled(profilingEnabled);
            TaskProfiler::Get().SetEnabled(profilingEnabled);
            Logger::Info("Profiling {}", profilingEnabled ? "enabled" : "disabled");
        }
    });

    orchestrator.SetUpdateCallback([&](EngineOrchestrator::FrameData &) {
    }); {
        using P = EngineOrchestrator::PhaseNames;

        RenderPassInfo voxelPass{};
        voxelPass.name = "Voxel Main";
        voxelPass.clearColor = true;
        voxelPass.clearDepth = true;

        IGraphicsContext *gfx{graphics.get()};

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

        for (auto &&[stage, nodes]: scheduler->GetStageNodes()) {
            if (stage == SystemStage::Render) {
                for (auto *node: nodes) {
                    const std::string &sysName{node->metadata.name};
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

    auto report = TaskProfiler::Get().GenerateReport();
    if (!report.empty()) {
        TaskProfiler::Get().SaveToFile("output/final_profiler_data.txt");
        Logger::Info("Profiler data saved");
    }

    Logger::Info("Application closing...");
    return 0;
}
