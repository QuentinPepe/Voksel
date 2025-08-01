import Core.Types;
import Core.Log;
import Tasks.TaskGraph;
import Tasks.TaskProfiler;
import Tasks.Orchestrator;
import Graphics.Window;
import Graphics;
import Input.Core;
import Input.Manager;
import Input.Window;
import std;

int main() {
    Logger::EnableColor(false);
    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig{};
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.title = "Voksel - Task Graph Example";
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

    EngineOrchestrator orchestrator(0);
    orchestrator.SetWindow(&window);
    orchestrator.SetInputManager(&inputManager);
    orchestrator.SetGraphicsContext(graphics.get());

    Vertex triangleVertices[] = {
        {{0.0f, 0.5f, 0.0f},   {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.0f},  {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };

    U32 vertexBuffer = graphics->CreateVertexBuffer(triangleVertices, sizeof(triangleVertices));

    ShaderCode vertexShader{};
    vertexShader.source = "basic/Basic.hlsl";
    vertexShader.entryPoint = "VSMain";
    vertexShader.stage = ShaderStage::Vertex;

    ShaderCode pixelShader{};
    pixelShader.source = "basic/Basic.hlsl";
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

    orchestrator.SetUserInputCallback([&](EngineOrchestrator::FrameData&) {

        if (inputManager.IsKeyJustPressed(Key::Escape)) {
            Logger::Info("ESC pressed, exiting...");
            shouldExit = true;
        }

        if (inputManager.IsKeyJustPressed(Key::F1)) {
            Logger::Debug("F1 key detected!");
            auto report = TaskProfiler::Get().GenerateReport();
            if (!report.empty()) {
                TaskProfiler::Get().SaveToFile("profiler_data.txt");
                Logger::Info("Profiler data saved");
            } else {
                Logger::Warn("No profiler data to save - profiling may be disabled");
            }
        }

        if (inputManager.IsKeyJustPressed(Key::F2)) {
            Logger::Debug("F2 key detected!");
            bool profilingEnabled = !orchestrator.IsProfilingEnabled();
            orchestrator.SetProfilingEnabled(profilingEnabled);
            TaskProfiler::Get().SetEnabled(profilingEnabled);
            Logger::Info("Profiling {}", profilingEnabled ? "enabled" : "disabled");
        }

    });

    orchestrator.SetRenderCallback([&](EngineOrchestrator::FrameData& frame) {
        RenderPassInfo passInfo{};
        passInfo.name = "Main Pass";
        passInfo.clearColor = true;
        passInfo.clearDepth = true;

        F32 time = static_cast<F32>(frame.totalTime);
        passInfo.clearColorValue[0] = (std::sin(time) + 1.0f) * 0.25f;
        passInfo.clearColorValue[1] = (std::sin(time * 1.3f) + 1.0f) * 0.25f;
        passInfo.clearColorValue[2] = (std::sin(time * 0.7f) + 1.0f) * 0.25f;
        passInfo.clearColorValue[3] = 1.0f;

        graphics->BeginRenderPass(passInfo);
        graphics->SetPipeline(pipeline);
        graphics->SetVertexBuffer(vertexBuffer);
        graphics->Draw(3);
        graphics->EndRenderPass();
    });

    U64 statsUpdateFrame = 0;
    orchestrator.SetPostFrameCallback([&](EngineOrchestrator::FrameData& frame) {
        if (frame.frameNumber - statsUpdateFrame >= 60) {
            statsUpdateFrame = frame.frameNumber;

            Logger::Info("Frame {} - FPS: {:.1f}, Frame Time: {:.2f}ms",
                        frame.frameNumber,
                        orchestrator.GetFPS(),
                        orchestrator.GetAverageFrameTime());

            auto stats = orchestrator.GetTaskGraphStats();
            Logger::Debug("Tasks: {} completed, {} failed, Execution time: {}μs",
                         stats.completedTasks, stats.failedTasks, stats.totalExecutionTime);
        }
    });

    {
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

    while (!graphics->ShouldClose() && !shouldExit) {
        orchestrator.ExecuteFrame();
    }

    Logger::Info("\n{}", TaskProfiler::Get().GenerateReport());

    {
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