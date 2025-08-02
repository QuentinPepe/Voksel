import Core.Types;
import Core.Log;
import Tasks.TaskGraph;
import Tasks.TaskProfiler;
import Tasks.Orchestrator;
import Tasks.ECSIntegration;
import Graphics;
import Graphics.Window;
import Graphics.RenderData;
import Graphics.DX12.GraphicsContext;
import Input.Core;
import Input.Manager;
import Input.Window;
import ECS.World;
import ECS.Component;
import ECS.Query;
import ECS.SystemScheduler;
import Components.ComponentRegistry;
import Components.Transform;
import Components.Camera;
import Components.CameraController;
import Systems.CameraManager;
import Systems.CameraSystem;
import Systems.CameraControllerSystem;
import Systems.RenderSystem;
import Math.Core;
import Math.Vector;
import Math.Matrix;

import std;

struct Position {
    F32 x, y, z;
};

struct Velocity {
    F32 x, y, z;
};

struct Health {
    F32 current;
    F32 max;
};

struct Enemy {
    F32 damage;
};

struct Player {
};

template<>
struct ComponentTypeID<Position> {
    static consteval ComponentID value() { return 1 + GAME_COMPONENT_START; }
};

template<>
struct ComponentTypeID<Velocity> {
    static consteval ComponentID value() { return 2 + GAME_COMPONENT_START; }
};

template<>
struct ComponentTypeID<Health> {
    static consteval ComponentID value() { return 3 + GAME_COMPONENT_START; }
};

template<>
struct ComponentTypeID<Enemy> {
    static consteval ComponentID value() { return 4 + GAME_COMPONENT_START; }
};

template<>
struct ComponentTypeID<Player> {
    static consteval ComponentID value() { return 5 + GAME_COMPONENT_START; }
};

class MovementSystem : public QuerySystem<MovementSystem, Write<Position>, Read<Velocity> > {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("Movement");
        SetStage(SystemStage::Update);
    }

    void Run(World *world, F32 dt) override {
        ForEach(world, [dt](Position *pos, const Velocity *vel) {
            pos->x += vel->x * dt;
            pos->y += vel->y * dt;
            pos->z += vel->z * dt;
        });
    }
};

class HealthSystem : public QuerySystem<HealthSystem, Write<Health> > {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("Health");
        SetStage(SystemStage::Update);
        RunAfter("Damage");
    }

    void Run(World *world, F32 dt) override {
        ForEach(world, [](Health *health) {
            health->current = std::clamp(health->current, 0.0f, health->max);
        });
    }
};

class DamageSystem : public QuerySystem<DamageSystem, Write<Health>, Read<Enemy>, Without<Player> > {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("Damage");
        SetStage(SystemStage::Update);
    }

    void Run(World *world, F32 dt) override {
        ForEach(world, [dt](Health *health, const Enemy *enemy) {
            health->current -= enemy->damage * dt;
        });
    }
};

class PlayerSystem : public QuerySystem<PlayerSystem, Read<Position>, Read<Health>, With<Player> > {
public:
    void Setup() {
        QuerySystem::Setup();
        SetName("Player");
        SetStage(SystemStage::PostUpdate);
        SetParallel(false);
    }

    void Run(World *world, F32 dt) override {
        ForEach(world, [](const Position *pos, const Health *health) {
        });
    }
};

int main() {
    Logger::EnableColor(false);
    Logger::Info("Starting the Voksel Engine with ECS v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig{};
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.title = "Voksel - ECS Example";
    windowConfig.fullscreen = false;
    Window window{windowConfig};

    GraphicsConfig graphicsConfig{};
    graphicsConfig.enableValidation = true;
    graphicsConfig.enableVSync = true;
    auto graphics = CreateGraphicsContext(window, graphicsConfig);

    InputManager inputManager;
    WindowInputHandler windowInput(&window, &inputManager);

    windowInput.SetResizeCallback([&graphics](U32 width, U32 height) {
        if (width > 0 && height > 0) {
            graphics->OnResize(width, height);
        }
    });

    World world;

    Logger::Info("Creating entities...");

    auto player = world.CreateEntity();
    world.AddComponent(player, Position{0.0f, 0.0f, 0.0f});
    world.AddComponent(player, Velocity{0.0f, 0.0f, 0.0f});
    world.AddComponent(player, Health{100.0f, 100.0f});
    world.AddComponent(player, Player{});

    for (int i = 0; i < 10; ++i) {
        auto enemy = world.CreateEntity();
        world.AddComponent(enemy, Position{
                               static_cast<F32>(i * 10.0f),
                               0.0f,
                               static_cast<F32>(i * 5.0f)
                           });
        world.AddComponent(enemy, Velocity{
                               static_cast<F32>((i % 3) - 1),
                               0.0f,
                               static_cast<F32>((i % 2) * 2 - 1)
                           });
        world.AddComponent(enemy, Health{50.0f, 50.0f});
        world.AddComponent(enemy, Enemy{10.0f});
    }

    EngineOrchestrator orchestrator(0);
    orchestrator.SetWindow(&window);
    orchestrator.SetInputManager(&inputManager);
    orchestrator.SetGraphicsContext(graphics.get());
    orchestrator.SetWorld(&world);

    EngineOrchestratorECS ecsIntegration(&orchestrator);
    auto *systemScheduler = ecsIntegration.GetSystemScheduler();

    auto *movementSystem = systemScheduler->AddSystem<MovementSystem>();
    auto *healthSystem = systemScheduler->AddSystem<HealthSystem>();
    auto *damageSystem = systemScheduler->AddSystem<DamageSystem>();
    auto *playerSystem = systemScheduler->AddSystem<PlayerSystem>();

    auto *cameraSystem = systemScheduler->AddSystem<CameraSystem>();
    auto *cameraLifecycleSystem = systemScheduler->AddSystem<CameraLifecycleSystem>();
    auto *cameraControllerSystem = systemScheduler->AddSystem<CameraControllerSystem>();
    cameraControllerSystem->SetInputManager(&inputManager);

    auto* renderDataSystem = systemScheduler->AddSystem<RenderDataSystem>();
    renderDataSystem->SetGraphicsContext(graphics.get());

    auto cameraEntity = world.CreateEntity();
    world.AddComponent(cameraEntity, Transform{Math::Vec3{0.0f, 5.0f, 10.0f}});
    world.AddComponent(cameraEntity, Camera{});
    world.AddComponent(cameraEntity, CameraController{});

    auto& cameraTransform = *world.GetComponent<Transform>(cameraEntity);
    cameraTransform.position = Math::Vec3{0.0f, 2.0f, 5.0f};
    cameraTransform.LookAt(Math::Vec3::Zero, Math::Vec3::Up);

    auto& cameraComp = *world.GetComponent<Camera>(cameraEntity);
    cameraComp.fov = Math::ToRadians(60.0f);
    cameraComp.aspectRatio = static_cast<F32>(windowConfig.width) / windowConfig.height;
    cameraComp.nearPlane = 0.1f;
    cameraComp.farPlane = 100.0f;
    cameraComp.UpdateProjection();

    ecsIntegration.BuildECSExecutionGraph(&world);

    windowInput.SetCursorLocked(false);
    windowInput.SetCursorVisible(true); {
        auto dotGraph = systemScheduler->GenerateDotGraph();
        std::ofstream file("ecs_systems.dot");
        if (file.is_open()) {
            file << dotGraph;
            Logger::Info("ECS system graph saved to ecs_systems.dot");
            Logger::Info("Generate PNG with: dot -Tpng ecs_systems.dot -o ecs_systems.png");
        }
    }

    Vertex triangleVertices[] = {
        {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };

    U32 vertexBuffer = graphics->CreateVertexBuffer(triangleVertices, sizeof(triangleVertices));

    ShaderCode vertexShader{};
    vertexShader.source = "basic\\Basic.hlsl";
    vertexShader.entryPoint = "VSMain";
    vertexShader.stage = ShaderStage::Vertex;

    ShaderCode pixelShader{};
    pixelShader.source = "basic\\Basic.hlsl";
    pixelShader.entryPoint = "PSMain";
    pixelShader.stage = ShaderStage::Pixel;

    GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.shaders = {vertexShader, pixelShader};
    pipelineInfo.vertexAttributes = {
        {"POSITION", 0},
        {"COLOR", 12}
    };
    pipelineInfo.vertexStride = sizeof(Vertex);
    pipelineInfo.topology = PrimitiveTopology::TriangleList;
    pipelineInfo.depthTest = false;
    pipelineInfo.depthWrite = false;

    U32 pipeline = graphics->CreateGraphicsPipeline(pipelineInfo);

    bool shouldExit = false;

    orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData &) {
        if (inputManager.IsKeyJustPressed(Key::Escape)) {
            Logger::Info("ESC pressed, exiting...");
            shouldExit = true;
        }

        if (inputManager.IsKeyJustPressed(Key::F1)) {
            auto report = TaskProfiler::Get().GenerateReport();
            if (!report.empty()) {
                TaskProfiler::Get().SaveToFile("profiler_data.txt");
                Logger::Info("Profiler data saved");
            }
        }

        if (inputManager.IsKeyJustPressed(Key::F2)) {
            bool profilingEnabled = !orchestrator.IsProfilingEnabled();
            orchestrator.SetProfilingEnabled(profilingEnabled);
            TaskProfiler::Get().SetEnabled(profilingEnabled);
            Logger::Info("Profiling {}", profilingEnabled ? "enabled" : "disabled");
        }

        if (inputManager.IsMouseButtonJustPressed(MouseButton::Right)) {
            windowInput.SetCursorLocked(true);
            windowInput.SetCursorVisible(false);
        }
        if (inputManager.IsMouseButtonJustReleased(MouseButton::Right)) {
            windowInput.SetCursorLocked(false);
            windowInput.SetCursorVisible(true);
        }
    });

    orchestrator.SetUpdateCallback([&](EngineOrchestrator::FrameData &frame) {
        ecsIntegration.UpdateECS(static_cast<F32>(frame.deltaTime));
    });

    orchestrator.SetRenderCallback([&](EngineOrchestrator::FrameData& frame) {
        RenderPassInfo passInfo{};
        passInfo.name = "Main Pass";
        passInfo.clearColor = true;
        passInfo.clearDepth = true;

        graphics->BeginRenderPass(passInfo);

        U32 cameraBuffer = renderDataSystem->GetCameraConstantBuffer();
        if (cameraBuffer != INVALID_INDEX) {
            graphics->SetConstantBuffer(cameraBuffer, 0);
        }

        static U32 objectBuffer = INVALID_INDEX;
        if (objectBuffer == INVALID_INDEX) {
            objectBuffer = graphics->CreateConstantBuffer(sizeof(ObjectConstants));
            ObjectConstants objectData{};
            objectData.world = Math::Mat4::Identity;
            graphics->UpdateConstantBuffer(objectBuffer, &objectData, sizeof(ObjectConstants));
        }
        graphics->SetConstantBuffer(objectBuffer, 1);

        graphics->SetPipeline(pipeline);
        graphics->SetVertexBuffer(vertexBuffer);
        graphics->Draw(3);

        graphics->EndRenderPass();
    });

    U64 statsUpdateFrame = 0;
    orchestrator.SetPostFrameCallback([&](EngineOrchestrator::FrameData &frame) {
        if (frame.frameNumber - statsUpdateFrame >= 60) {
            statsUpdateFrame = frame.frameNumber;

            Logger::Debug("Frame {} - FPS: {:.1f}, Frame Time: {:.2f}ms, Entities: {}",
                          frame.frameNumber,
                          orchestrator.GetFPS(),
                          orchestrator.GetAverageFrameTime(),
                          world.GetEntityCount());

            auto stats = orchestrator.GetTaskGraphStats();
            Logger::Debug("Tasks: {} completed, {} failed, Execution time: {}μs",
                          stats.completedTasks, stats.failedTasks, stats.totalExecutionTime);

            auto ecsStats = systemScheduler->GetStats();
            if (!ecsStats.systemTimes.empty()) {
                Logger::Debug("Top ECS Systems:");
                for (size_t i = 0; i < std::min<size_t>(3, ecsStats.systemTimes.size()); ++i) {
                    Logger::Debug("  {}: {}μs",
                                  ecsStats.systemTimes[i].first,
                                  ecsStats.systemTimes[i].second);
                }
            }
        }
    }); {
        auto dotGraph = orchestrator.GenerateTaskGraphVisualization();
        std::ofstream file("task_graph.dot");
        if (file.is_open()) {
            file << dotGraph;
            Logger::Info("Task graph visualization saved to task_graph.dot");
            Logger::Info("Generate PNG with: dot -Tpng task_graph.dot -o task_graph.png");
        }
    }

    Logger::Info("Starting render loop...");
    Logger::Info("Press ESC to exit, F1 to save profiler data, F2 to toggle profiling");
    Logger::Info("Press SPACE to spawn a new entity");

    while (!graphics->ShouldClose() && !shouldExit) {
        orchestrator.ExecuteFrame();
    }

    Logger::Info("\n{}", TaskProfiler::Get().GenerateReport()); {
        auto dotGraph = orchestrator.GenerateTaskGraphVisualization();
        std::ofstream file("task_graph_final.dot");
        if (file.is_open()) {
            file << dotGraph;
            Logger::Info("Final task graph saved to task_graph_final.dot");
        }
    }

    Logger::Info("Application closing...");

    return 0;
}
