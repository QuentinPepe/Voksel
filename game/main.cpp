import Core.Types;
import Core.Log;
import Tasks.TaskProfiler;
import Graphics.Window;
import Graphics;
import Input.Core;
import Input.Manager;
import Input.Window;
import std;

int main() {
    Logger::SetLevel(LogLevel::Debug);
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

    // TODO ORCHESTRATOR

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

    while (!graphics->ShouldClose() && !shouldExit) {

    }

    Logger::Info("Application closing...");

    return 0;
}