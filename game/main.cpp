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
import Systems.Hotbar;

import Math.Core;
import Math.Vector;

import Tasks.Orchestrator;
import Tasks.ECSIntegration;
import Tasks.TaskGraph;
import Tasks.TaskProfiler;

import UI.Core;
import UI.Element;
import UI.Layout;
import UI.Widgets;
import UI.Manager;

import Utils.String;

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
   gc.enableVSync = false;

   auto graphics{CreateGraphicsContext(window, gc)};
   assert(graphics != nullptr, "Graphics creation failed");

   InputManager inputManager{};
   WindowInputHandler windowInput{&window, &inputManager};

   UIManager uiManager{graphics.get(), &inputManager};
   uiManager.SetScreenSize(static_cast<F32>(wc.width), static_cast<F32>(wc.height));
   uiManager.SetFontFromTTF("assets/fonts/Roboto-Regular.ttf", 48.0f);

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

   auto crosshair{uiManager.CreatePanel("Crosshair")};
   crosshair->SetAnchor(AnchorPreset::Center);
   crosshair->SetPivot({0.5f, 0.5f});
   crosshair->SetSizeDelta({2.0f, 20.0f});
   std::static_pointer_cast<UIPanel>(crosshair)->SetBackgroundColor(Color{1.0f, 1.0f, 1.0f, 0.8f});
   uiManager.GetRoot()->AddChild(crosshair);

   auto crosshairH{uiManager.CreatePanel("CrosshairH")};
   crosshairH->SetAnchor(AnchorPreset::Center);
   crosshairH->SetPivot({0.5f, 0.5f});
   crosshairH->SetSizeDelta({20.0f, 2.0f});
   std::static_pointer_cast<UIPanel>(crosshairH)->SetBackgroundColor(Color{1.0f, 1.0f, 1.0f, 0.8f});
   uiManager.GetRoot()->AddChild(crosshairH);

   auto debugPanel{uiManager.CreatePanel("DebugPanel")};
   debugPanel->SetAnchor(AnchorPreset::TopLeft);
   debugPanel->SetPivot({0.0f, 0.0f});
   debugPanel->SetAnchoredPosition({10.0f, 10.0f});
   debugPanel->SetSizeDelta({200.0f, 140.0f});
   std::static_pointer_cast<UIPanel>(debugPanel)->SetBackgroundColor(Color{0.0f, 0.0f, 0.0f, 0.7f});
   uiManager.GetRoot()->AddChild(debugPanel);

   auto debugLayout{uiManager.CreateVerticalLayout()};
   auto v{std::static_pointer_cast<UIVerticalLayout>(debugLayout)};
   v->SetPadding(Margin{5.0f});
   v->SetSpacing(2.0f);
   v->SetChildControl(true, false);
   v->SetChildForceExpand(false, false);
   debugPanel->AddChild(debugLayout);

   auto sized = [](UIElementPtr e, float h){
       e->SetAnchor(AnchorPreset::TopLeft);
       e->SetPivot({0,0});
       e->SetSizeDelta({190.0f, h});
   };

   auto fpsText{uiManager.CreateText("FPS: 60")};
   std::static_pointer_cast<UIText>(fpsText)->SetTextColor(Color{0,1,0,1});
   sized(fpsText, 16.0f);  v->AddChild(fpsText);

   auto posText{uiManager.CreateText("Pos: 0, 0, 0")};
   sized(posText, 16.0f);  v->AddChild(posText);

   auto chunkText{uiManager.CreateText("Chunks: 0")};
   sized(chunkText, 16.0f); v->AddChild(chunkText);

   auto visText{uiManager.CreateText("Visible: 0")};
   sized(visText, 16.0f); v->AddChild(visText);

   auto culledText{uiManager.CreateText("Culled: 0 / 0")};
   sized(culledText, 16.0f); v->AddChild(culledText);

   auto drawsText{uiManager.CreateText("Draws: 0  Vtx/Idx: 0/0")};
   sized(drawsText, 16.0f); v->AddChild(drawsText);

   windowInput.SetResizeCallback([&graphics, &uiManager](U32 w, U32 h) {
       if (w > 0 && h > 0) {
           graphics->OnResize(w, h);
           uiManager.SetScreenSize(static_cast<F32>(w), static_cast<F32>(h));
       }
   });

   windowInput.SetCursorLocked(false);
   windowInput.SetCursorVisible(true);

   EngineOrchestrator orchestrator{0};
   orchestrator.SetInputManager(&inputManager);
   orchestrator.SetWindow(&window);
   orchestrator.SetWorld(&world);
   orchestrator.SetGraphicsContext(graphics.get());
   orchestrator.SetFrameLimit(144);

   EngineOrchestratorECS orchestratorECS{&orchestrator};
   SystemScheduler* scheduler{orchestratorECS.GetSystemScheduler()};

   auto* cameraSystem{scheduler->AddSystem<CameraSystem>()};
   auto* cameraLifecycle{scheduler->AddSystem<CameraLifecycleSystem>()};
   auto* cameraController{scheduler->AddSystem<CameraControllerSystem>()};
   cameraController->SetInputManager(&inputManager);
   cameraController->SetWindowInputHandler(&windowInput);

   auto* voxelStreamer{scheduler->AddSystem<VoxelStreamingSystem>()};
   auto* voxelGen{scheduler->AddSystem<VoxelGenerationSystem>()};
   auto* voxelMesher{scheduler->AddSystem<VoxelMeshingSystem>()};
   auto* voxelUpload{scheduler->AddSystem<VoxelUploadSystem>()};
   auto* voxelRenderer{scheduler->AddSystem<VoxelRendererSystem>()};
   auto* voxelEdit{scheduler->AddSystem<VoxelEditSystem>()};
   voxelEdit->SetInputManager(&inputManager);
   auto* voxelSel{scheduler->AddSystem<VoxelSelectionRenderSystem>()};

   auto* hotbar{scheduler->AddSystem<HotbarSystem>()};
   hotbar->SetUIManager(&uiManager);
   hotbar->SetInputManager(&inputManager);
   hotbar->OnResize(wc.width, wc.height);

   voxelUpload->SetGraphicsContext(graphics.get());
   voxelRenderer->SetGraphicsContext(graphics.get());
   voxelSel->SetGraphicsContext(graphics.get());

   orchestratorECS.BuildECSExecutionGraph(&world);

   orchestrator.SetProfilingEnabled(true);
   TaskProfiler::Get().SetEnabled(true);

   F32 frameTime{0.0f};
   U32 frameCount{0};
   F32 fpsTimer{0.0f};
   U32 currentFPS{60};

   orchestrator.SetPreFrameCallback([&](EngineOrchestrator::FrameData& frame) {
       frameTime = static_cast<F32>(frame.deltaTime);
       frameCount++;
       fpsTimer += frameTime;

       if (fpsTimer >= 1.0f) {
           F32 fpsVal{static_cast<F32>(frameCount) / fpsTimer};
           currentFPS = static_cast<U32>(fpsVal + 0.5f);
           frameCount = 0;
           fpsTimer = 0.0f;
           std::static_pointer_cast<UIText>(fpsText)->SetText(std::string{"FPS: "} + Utils::ToString(currentFPS));
       }

       if (auto* transform{world.GetComponent<Transform>(cameraEntity)}) {
           std::string posStr{"Pos: "};
           posStr += Utils::ToString(static_cast<S32>(transform->position.x));
           posStr += ", ";
           posStr += Utils::ToString(static_cast<S32>(transform->position.y));
           posStr += ", ";
           posStr += Utils::ToString(static_cast<S32>(transform->position.z));
           std::static_pointer_cast<UIText>(posText)->SetText(posStr);
       }

       auto* chunkStore{world.GetStorage<VoxelChunk>()};
       if (chunkStore) {
           std::static_pointer_cast<UIText>(chunkText)->SetText(std::string{"Chunks: "} + Utils::ToString(chunkStore->Size()));
       }

       if (auto* sStore{world.GetStorage<VoxelCullingStats>()}; sStore && sStore->Size() > 0) {
           VoxelCullingStats s{};
           for (auto [h, cs] : *sStore) { s = cs; break; }
           std::static_pointer_cast<UIText>(visText)->SetText(std::string{"Visible: "} + Utils::ToString(s.visible));
           std::static_pointer_cast<UIText>(culledText)->SetText(std::string{"Culled: "} + Utils::ToString(s.culled) + " / " + Utils::ToString(s.tested));
           std::static_pointer_cast<UIText>(drawsText)->SetText(std::string{"Draws: "} + Utils::ToString(s.drawCalls) + "  Vtx/Idx: " + Utils::ToString(static_cast<U64>(s.drawnVerts)) + "/" + Utils::ToString(static_cast<U64>(s.drawnIndices)));
       }

       uiManager.Update(frameTime);
       orchestratorECS.UpdateECS(frameTime);
   });

   orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData&) {
       if (inputManager.IsKeyJustPressed(Key::Escape)) {
           window.RequestClose();
       }

       if (inputManager.IsKeyJustPressed(Key::F3)) {
           bool isVisible{debugPanel->IsVisible()};
           debugPanel->SetVisibility(isVisible ? Visibility::Hidden : Visibility::Visible);
       }

       if (inputManager.IsKeyJustPressed(Key::F2)) {
           auto report{TaskProfiler::Get().GenerateReport()};
           if (!report.empty()) {
               TaskProfiler::Get().SaveToFile("output/profiler_data.txt");
               Logger::Info("Profiler data saved");
           }
       }

       if (inputManager.IsKeyJustPressed(Key::F5)) {
           bool profilingEnabled{!orchestrator.IsProfilingEnabled()};
           orchestrator.SetProfilingEnabled(profilingEnabled);
           TaskProfiler::Get().SetEnabled(profilingEnabled);
           Logger::Info("Profiling {}", profilingEnabled ? "enabled" : "disabled");
       }
   });

   orchestrator.SetUpdateCallback([&](EngineOrchestrator::FrameData&) {});

   {
       using P = EngineOrchestrator::PhaseNames;

       RenderPassInfo voxelPass{};
       voxelPass.name = "Voxel Main";
       voxelPass.clearColor = true;
       voxelPass.clearDepth = true;

       IGraphicsContext* gfx{graphics.get()};

       orchestrator.AddTaskToPhase(
           P::Render,
           "BeginVoxelPass",
           [gfx, voxelPass]() mutable { gfx->BeginRenderPass(voxelPass); },
           TaskPriority::High
       );

       orchestrator.AddTaskToPhase(
           P::Render,
           "RenderUI",
           [&uiManager]() { uiManager.Render(); },
           TaskPriority::Low
       );

       orchestrator.AddTaskToPhase(
           P::Render,
           "EndVoxelPass",
           [gfx]() { gfx->EndRenderPass(); },
           TaskPriority::Low
       );

       for (auto&& [stage, nodes] : scheduler->GetStageNodes()) {
           if (stage == SystemStage::Render) {
               for (auto* node : nodes) {
                   const std::string& sysName{node->metadata.name};
                   orchestrator.AddTaskDependency(P::Render, sysName, "BeginVoxelPass");
                   orchestrator.AddTaskDependency(P::Render, "RenderUI", sysName);
               }
           }
       }

       orchestrator.AddTaskDependency(P::Render, "EndVoxelPass", "RenderUI");
   }

   Logger::Info("Starting render loop – ESC quits");

   while (!graphics->ShouldClose()) {
       windowInput.Flush();
       orchestrator.ExecuteFrame();
   }

   auto report{TaskProfiler::Get().GenerateReport()};
   if (!report.empty()) {
       TaskProfiler::Get().SaveToFile("output/final_profiler_data.txt");
       Logger::Info("Profiler data saved");
   }

   Logger::Info("Application closing...");
   return 0;
}
