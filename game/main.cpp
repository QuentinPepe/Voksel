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
   gc.enableVSync = true;

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

   auto hotbarContainer = uiManager.CreatePanel("HotbarContainer");
   hotbarContainer->SetAnchor(AnchorPreset::BottomCenter);
   hotbarContainer->SetPivot({0.5f, 1.0f});
   hotbarContainer->SetAnchoredPosition({0.0f, -20.0f});
   hotbarContainer->SetSizeDelta({450.0f, 60.0f});
   uiManager.GetRoot()->AddChild(hotbarContainer);

   auto hotbarLayout = uiManager.CreateHorizontalLayout();
   hotbarLayout->SetAnchor(AnchorPreset::StretchAll);
   hotbarLayout->SetMargin(Margin{5.0f});
   std::static_pointer_cast<UIHorizontalLayout>(hotbarLayout)->SetSpacing(5.0f);
   std::static_pointer_cast<UIHorizontalLayout>(hotbarLayout)->SetChildControl(false, true);
   std::static_pointer_cast<UIHorizontalLayout>(hotbarLayout)->SetChildAlignment(Alignment::Center);
   hotbarContainer->AddChild(hotbarLayout);

   enum class BlockType : U8 {
       Dirt = 1,
       Grass = 2,
       Stone = 3,
       Wood = 4,
       Leaves = 5,
       Sand = 6,
       Glass = 7,
       Brick = 8,
       Water = 9
   };

   BlockType selectedBlock{BlockType::Dirt};
   S32 selectedSlot{0};

   struct HotbarSlot {
       UIElementPtr panel;
       UIElementPtr icon;
       UIElementPtr countText;
       UIElementPtr selectionBorder;
       BlockType blockType;
       U32 count;
   };

   Vector<HotbarSlot> hotbarSlots{};

   const Vector<std::pair<BlockType, std::string>> blockData{
       {BlockType::Dirt, "Dirt"},
       {BlockType::Grass, "Grass"},
       {BlockType::Stone, "Stone"},
       {BlockType::Wood, "Wood"},
       {BlockType::Leaves, "Leaves"},
       {BlockType::Sand, "Sand"},
       {BlockType::Glass, "Glass"},
       {BlockType::Brick, "Brick"},
       {BlockType::Water, "Water"}
   };

   for (USize i = 0; i < 9; ++i) {
       auto slotPanel = uiManager.CreatePanel();
       slotPanel->SetSizeDelta({44.0f, 44.0f});
       std::static_pointer_cast<UIPanel>(slotPanel)->SetBackgroundColor(Color{0.15f, 0.15f, 0.15f, 0.9f});
       std::static_pointer_cast<UIPanel>(slotPanel)->SetBorderWidth(2.0f);
       std::static_pointer_cast<UIPanel>(slotPanel)->SetBorderColor(Color{0.3f, 0.3f, 0.3f, 1.0f});

       auto iconPanel = uiManager.CreatePanel();
       iconPanel->SetAnchor(AnchorPreset::Center);
       iconPanel->SetPivot({0.5f, 0.5f});
       iconPanel->SetSizeDelta({32.0f, 32.0f});

       Color blockColor{0.5f, 0.5f, 0.5f, 1.0f};
       if (i < blockData.size()) {
           switch (blockData[i].first) {
               case BlockType::Dirt: blockColor = Color{0.52f, 0.37f, 0.26f, 1.0f}; break;
               case BlockType::Grass: blockColor = Color{0.37f, 0.62f, 0.21f, 1.0f}; break;
               case BlockType::Stone: blockColor = Color{0.5f, 0.5f, 0.5f, 1.0f}; break;
               case BlockType::Wood: blockColor = Color{0.64f, 0.42f, 0.2f, 1.0f}; break;
               case BlockType::Leaves: blockColor = Color{0.2f, 0.6f, 0.2f, 0.9f}; break;
               case BlockType::Sand: blockColor = Color{0.9f, 0.8f, 0.5f, 1.0f}; break;
               case BlockType::Glass: blockColor = Color{0.7f, 0.9f, 1.0f, 0.4f}; break;
               case BlockType::Brick: blockColor = Color{0.6f, 0.25f, 0.15f, 1.0f}; break;
               case BlockType::Water: blockColor = Color{0.2f, 0.4f, 0.8f, 0.7f}; break;
           }
       }
       std::static_pointer_cast<UIPanel>(iconPanel)->SetBackgroundColor(blockColor);
       slotPanel->AddChild(iconPanel);

       auto countText = uiManager.CreateText("64");
       countText->SetAnchor(AnchorPreset::BottomRight);
       countText->SetPivot({1.0f, 1.0f});
       countText->SetAnchoredPosition({-2.0f, -2.0f});
       countText->SetSizeDelta({20.0f, 12.0f});
       std::static_pointer_cast<UIText>(countText)->SetFontSize(10);
       std::static_pointer_cast<UIText>(countText)->SetTextColor(Color::White);
       std::static_pointer_cast<UIText>(countText)->SetAlignment(Alignment::End, Alignment::End);
       slotPanel->AddChild(countText);

       auto selectionBorder = uiManager.CreatePanel();
       selectionBorder->SetAnchor(AnchorPreset::StretchAll);
       selectionBorder->SetMargin(Margin{-2.0f});
       std::static_pointer_cast<UIPanel>(selectionBorder)->SetBackgroundColor(Color::Transparent);
       std::static_pointer_cast<UIPanel>(selectionBorder)->SetBorderWidth(3.0f);
       std::static_pointer_cast<UIPanel>(selectionBorder)->SetBorderColor(Color{1.0f, 1.0f, 0.0f, 1.0f});
       selectionBorder->SetVisibility(i == 0 ? Visibility::Visible : Visibility::Hidden);
       slotPanel->AddChild(selectionBorder);

       hotbarLayout->AddChild(slotPanel);

       HotbarSlot slot{};
       slot.panel = slotPanel;
       slot.icon = iconPanel;
       slot.countText = countText;
       slot.selectionBorder = selectionBorder;
       slot.blockType = i < blockData.size() ? blockData[i].first : BlockType::Dirt;
       slot.count = 64;
       hotbarSlots.push_back(slot);
   }

   auto updateHotbar = [&](U32 w, U32) {
       constexpr F32 kSlot{44.0f};
       constexpr F32 kBaseSpacing{5.0f};
       constexpr F32 kBasePadding{5.0f};
       constexpr U32 kCount{9};
       F32 baseW = kCount * kSlot + (kCount - 1) * kBaseSpacing + 2.0f * kBasePadding;
       F32 maxW = std::max(0.0f, static_cast<F32>(w) - 40.0f);
       F32 scale = std::min(1.0f, maxW / baseW);
       F32 spacing = kBaseSpacing * scale;
       F32 padding = kBasePadding * scale;
       F32 slot = kSlot * scale;
       F32 contW = kCount * slot + (kCount - 1) * spacing + 2.0f * padding;
       F32 contH = 60.0f * scale;
       hotbarContainer->SetSizeDelta({contW, contH});
       hotbarLayout->SetMargin(Margin{padding});
       std::static_pointer_cast<UIHorizontalLayout>(hotbarLayout)->SetSpacing(spacing);
       assert(hotbarSlots.size() == kCount, "hotbarSlots size mismatch");
       for (auto& s : hotbarSlots) {
           std::static_pointer_cast<UIPanel>(s.panel)->SetBorderWidth(2.0f * scale);
           s.panel->SetSizeDelta({slot, slot});
           s.icon->SetSizeDelta({32.0f * scale, 32.0f * scale});
           s.countText->SetAnchoredPosition({-2.0f * scale, -2.0f * scale});
           std::static_pointer_cast<UIText>(s.countText)->SetFontSize(static_cast<U32>(std::max(8.0f, 10.0f * scale)));
           std::static_pointer_cast<UIPanel>(s.selectionBorder)->SetBorderWidth(3.0f * scale);
       }
   };

   updateHotbar(wc.width, wc.height);

   auto crosshair = uiManager.CreatePanel("Crosshair");
   crosshair->SetAnchor(AnchorPreset::Center);
   crosshair->SetPivot({0.5f, 0.5f});
   crosshair->SetSizeDelta({2.0f, 20.0f});
   std::static_pointer_cast<UIPanel>(crosshair)->SetBackgroundColor(Color{1.0f, 1.0f, 1.0f, 0.8f});
   uiManager.GetRoot()->AddChild(crosshair);

   auto crosshairH = uiManager.CreatePanel("CrosshairH");
   crosshairH->SetAnchor(AnchorPreset::Center);
   crosshairH->SetPivot({0.5f, 0.5f});
   crosshairH->SetSizeDelta({20.0f, 2.0f});
   std::static_pointer_cast<UIPanel>(crosshairH)->SetBackgroundColor(Color{1.0f, 1.0f, 1.0f, 0.8f});
   uiManager.GetRoot()->AddChild(crosshairH);

   auto debugPanel = uiManager.CreatePanel("DebugPanel");
   debugPanel->SetAnchor(AnchorPreset::TopLeft);
   debugPanel->SetPivot({0.0f, 0.0f});
   debugPanel->SetAnchoredPosition({10.0f, 10.0f});
   debugPanel->SetSizeDelta({200.0f, 100.0f});
   std::static_pointer_cast<UIPanel>(debugPanel)->SetBackgroundColor(Color{0.0f, 0.0f, 0.0f, 0.7f});
   uiManager.GetRoot()->AddChild(debugPanel);

   auto debugLayout = uiManager.CreateVerticalLayout();
   debugLayout->SetAnchor(AnchorPreset::StretchAll);
   debugLayout->SetMargin(Margin{5.0f});
   std::static_pointer_cast<UIVerticalLayout>(debugLayout)->SetSpacing(2.0f);
   debugPanel->AddChild(debugLayout);

   auto fpsText = uiManager.CreateText("FPS: 60");
   std::static_pointer_cast<UIText>(fpsText)->SetTextColor(Color{0.0f, 1.0f, 0.0f, 1.0f});
   std::static_pointer_cast<UIText>(fpsText)->SetAlignment(Alignment::Start, Alignment::Center);
   debugLayout->AddChild(fpsText);

   auto posText = uiManager.CreateText("Pos: 0, 0, 0");
   std::static_pointer_cast<UIText>(posText)->SetTextColor(Color::White);
   std::static_pointer_cast<UIText>(posText)->SetAlignment(Alignment::Start, Alignment::Center);
   debugLayout->AddChild(posText);

   auto chunkText = uiManager.CreateText("Chunks: 0");
   std::static_pointer_cast<UIText>(chunkText)->SetTextColor(Color::White);
   std::static_pointer_cast<UIText>(chunkText)->SetAlignment(Alignment::Start, Alignment::Center);
   debugLayout->AddChild(chunkText);

   windowInput.SetResizeCallback([&graphics, &uiManager, &updateHotbar](U32 w, U32 h) {
       if (w > 0 && h > 0) {
           graphics->OnResize(w, h);
           uiManager.SetScreenSize(static_cast<F32>(w), static_cast<F32>(h));
           updateHotbar(w, h);
       }
   });

   windowInput.SetCursorLocked(false);
   windowInput.SetCursorVisible(true);

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

   orchestratorECS.BuildECSExecutionGraph(&world);

   orchestrator.SetProfilingEnabled(true);
   TaskProfiler::Get().SetEnabled(true);

   F32 frameTime{0.0f};
   U32 frameCount{0};
   F32 fpsTimer{0.0f};
   U32 currentFPS{60};

   orchestrator.SetPreFrameCallback([&](EngineOrchestrator::FrameData &frame) {
       frameTime = static_cast<F32>(frame.deltaTime);
       frameCount++;
       fpsTimer += frameTime;

       if (fpsTimer >= 1.0f) {
           F32 fpsVal = static_cast<F32>(frameCount) / fpsTimer;
           currentFPS = static_cast<U32>(fpsVal + 0.5f);
           frameCount = 0;
           fpsTimer = 0.0f;
           std::static_pointer_cast<UIText>(fpsText)->SetText(std::string{"FPS: "} + Utils::ToString(currentFPS));
       }

       if (auto* transform = world.GetComponent<Transform>(cameraEntity)) {
           std::string posStr{"Pos: "};
           posStr += Utils::ToString(static_cast<S32>(transform->position.x));
           posStr += ", ";
           posStr += Utils::ToString(static_cast<S32>(transform->position.y));
           posStr += ", ";
           posStr += Utils::ToString(static_cast<S32>(transform->position.z));
           std::static_pointer_cast<UIText>(posText)->SetText(posStr);
       }

       auto* chunkStore = world.GetStorage<VoxelChunk>();
       if (chunkStore) {
           std::static_pointer_cast<UIText>(chunkText)->SetText(std::string{"Chunks: "} + Utils::ToString(chunkStore->Size()));
       }

       uiManager.Update(frameTime);
       orchestratorECS.UpdateECS(frameTime);
   });

   orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData &) {
       if (inputManager.IsKeyJustPressed(Key::Escape)) {
           window.RequestClose();
       }

       for (S32 i = 0; i < 9; ++i) {
           if (inputManager.IsKeyJustPressed(static_cast<Key>(static_cast<U16>(Key::Num1) + i))) {
               hotbarSlots[selectedSlot].selectionBorder->SetVisibility(Visibility::Hidden);
               selectedSlot = i;
               selectedBlock = hotbarSlots[i].blockType;
               hotbarSlots[selectedSlot].selectionBorder->SetVisibility(Visibility::Visible);
           }
       }

       F32 scroll = inputManager.GetMouseScroll().second;
       if (scroll != 0.0f) {
           hotbarSlots[selectedSlot].selectionBorder->SetVisibility(Visibility::Hidden);
           if (scroll > 0) {
               selectedSlot = (selectedSlot + 1) % 9;
           } else {
               selectedSlot = (selectedSlot + 8) % 9;
           }
           selectedBlock = hotbarSlots[selectedSlot].blockType;
           hotbarSlots[selectedSlot].selectionBorder->SetVisibility(Visibility::Visible);
       }

       if (inputManager.IsKeyJustPressed(Key::F1)) {
           bool isVisible = debugPanel->IsVisible();
           debugPanel->SetVisibility(isVisible ? Visibility::Hidden : Visibility::Visible);
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

       if (inputManager.IsKeyJustPressed(Key::H)) {
           bool isVisible = hotbarContainer->IsVisible();
           hotbarContainer->SetVisibility(isVisible ? Visibility::Hidden : Visibility::Visible);
           crosshair->SetVisibility(isVisible ? Visibility::Hidden : Visibility::Visible);
           crosshairH->SetVisibility(isVisible ? Visibility::Hidden : Visibility::Visible);
       }
   });

   orchestrator.SetUpdateCallback([&](EngineOrchestrator::FrameData &) {
   });

   {
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
           "RenderUI",
           [&uiManager]() {
               uiManager.Render();
           },
           TaskPriority::Low
       );

       orchestrator.AddTaskToPhase(
           P::Render,
           "EndVoxelPass",
           [gfx]() {
               gfx->EndRenderPass();
           },
           TaskPriority::Low
       );

       for (auto &&[stage, nodes]: scheduler->GetStageNodes()) {
           if (stage == SystemStage::Render) {
               for (auto *node: nodes) {
                   const std::string &sysName{node->metadata.name};
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

   auto report = TaskProfiler::Get().GenerateReport();
   if (!report.empty()) {
       TaskProfiler::Get().SaveToFile("output/final_profiler_data.txt");
       Logger::Info("Profiler data saved");
   }

   Logger::Info("Application closing...");
   return 0;
}
