import ECS.Query;
import ECS.Component;
import ECS.World;
import Core.Types;
import Core.Log;
import Graphics.Window;
import std;


int main() {
    Logger::SetLevel(LogLevel::Debug);
    Logger::SetFile("voxel_engine.log");

    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig{};
    windowConfig.width = 1920;
    windowConfig.height = 1080;
    windowConfig.title = "VokselEngine - Game";

    Window window{windowConfig};

    while (!window.ShouldClose()) {
        window.PollEvents();
    }

    return 0;
}
