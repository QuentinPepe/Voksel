import ECS.Query;
import ECS.Component;
import ECS.World;
import Core.Types;
import Core.Log;
import Graphics.Window;
import Graphics.Renderer;
import Graphics.Mesh;
import Graphics.RenderPipeline;
import std;


int main() {
    Logger::SetLevel(LogLevel::Debug);
    Logger::SetFile("voxel_engine.log");

    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig{};
    windowConfig.width = 1920;
    windowConfig.height = 1080;
    windowConfig.title = "VokselEngine - Triangle Demo";

    Window window{windowConfig};
    Renderer renderer{&window};

    auto* device = renderer.GetDX11Backend()->GetDevice();
    auto* context = renderer.GetDX11Backend()->GetContext();
    RenderPipeline pipeline{device, context};

    auto triangleMesh = CreateTriangleMesh(device);

    U32 lastWidth = window.GetWidth();
    U32 lastHeight = window.GetHeight();

    Logger::Info("Rendering triangle...");

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.GetWidth() != lastWidth || window.GetHeight() != lastHeight) {
            lastWidth = window.GetWidth();
            lastHeight = window.GetHeight();
            renderer.OnResize(lastWidth, lastHeight);
        }

        renderer.BeginFrame();
        pipeline.BeginFrame();

        pipeline.DrawMesh(*triangleMesh);

        renderer.EndFrame();
    }

    Logger::Info("Shutting down...");

    return 0;
}
