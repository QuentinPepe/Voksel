import Core.Types;
import Core.Log;
import Graphics.Window;
import Graphics;
import std;

int main() {
    Logger::SetLevel(LogLevel::Debug);
    // Logger::SetFile("voxel_engine.log");

    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig;
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.title = "Voksel - Triangle Example";
    windowConfig.fullscreen = false;
    Window window{windowConfig};

    GraphicsConfig graphicsConfig;
    graphicsConfig.enableValidation = true;
    graphicsConfig.enableVSync = true;
    auto graphics = CreateGraphicsContext(window, graphicsConfig);

    Vertex triangleVertices[] = {
        {{0.0f, 0.5f, 0.0f},   {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.0f},  {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };

    U32 vertexBuffer = graphics->CreateVertexBuffer(triangleVertices, sizeof(triangleVertices));

    ShaderCode vertexShader;
    vertexShader.source = R"(
struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color    = input.color;
    return output;
}
)";
    vertexShader.entryPoint = "main";
    vertexShader.stage = ShaderStage::Vertex;

    ShaderCode pixelShader;
    pixelShader.source = R"(
struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return input.color;
}
)";
    pixelShader.entryPoint = "main";
    pixelShader.stage = ShaderStage::Pixel;

    GraphicsPipelineCreateInfo pipelineInfo;
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

    Logger::Info("Starting render loop...");

    while (!graphics->ShouldClose()) {
        window.PollEvents();

        graphics->BeginFrame();

        RenderPassInfo passInfo;
        passInfo.name = "Main Pass";
        passInfo.clearColor = true;
        passInfo.clearDepth = true;
        passInfo.clearColorValue[0] = 0.0f;
        passInfo.clearColorValue[1] = 0.2f;
        passInfo.clearColorValue[2] = 0.4f;
        passInfo.clearColorValue[3] = 1.0f;

        graphics->BeginRenderPass(passInfo);
        graphics->SetPipeline(pipeline);
        graphics->SetVertexBuffer(vertexBuffer);
        graphics->Draw(3);
        graphics->EndRenderPass();

        graphics->EndFrame();
    }

    Logger::Info("Application closing...");

    return 0;
}