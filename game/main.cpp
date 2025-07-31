import ECS.Query;
import ECS.Component;
import ECS.World;
import Core.Types;
import Core.Log;
import Graphics.Window;
import Graphics.Renderer;
import Graphics.Mesh;
import Graphics.RenderPipeline;
import Input.Core;
import Input.Manager;
import Input.Bindings;
import Input.Window;
import std;

class DebugInputListener {
public:
    void OnKeyEvent(const KeyEvent& event) {
        if (event.action == InputEventAction::Press) {
            Logger::Debug("Key pressed: {} (scancode: {})", GetKeyName(event.key), event.scancode);
        }
    }

    void OnMouseButtonEvent(const MouseButtonEvent& event) {
        if (event.action == InputEventAction::Press) {
            Logger::Debug("Mouse button pressed: {} at ({}, {})",
                         GetMouseButtonName(event.button), event.x, event.y);
        }
    }

    void OnMouseMoveEvent(const MouseMoveEvent& event) {
    }

    void OnMouseScrollEvent(const MouseScrollEvent& event) {
        Logger::Debug("Mouse scroll: ({}, {})", event.xOffset, event.yOffset);
    }

    void OnTextInputEvent(const TextInputEvent& event) {
        Logger::Debug("Text input: U+{:04X}", event.codepoint);
    }
};

int main() {
    Logger::SetLevel(LogLevel::Debug);
    Logger::SetFile("voxel_engine.log");

    Logger::Info("Starting the Voksel Engine v{}.{}.{}", 0, 1, 0);

    WindowConfig windowConfig{};
    windowConfig.width = 1920;
    windowConfig.height = 1080;
    windowConfig.title = "VokselEngine - Input System Demo";

    Window window{windowConfig};
    Renderer renderer{&window};

    auto* device = renderer.GetDX11Backend()->GetDevice();
    auto* context = renderer.GetDX11Backend()->GetContext();
    RenderPipeline pipeline{device, context};

    auto triangleMesh = CreateTriangleMesh(device);

    InputManager inputManager;
    WindowInputHandler windowInput{&window, &inputManager};

    InputMapper inputMapper;

    auto gameContext = inputMapper.CreateContext("Game", 10);
    auto uiContext = inputMapper.CreateContext("UI", 20);

    inputMapper.SetupFPSControls(gameContext);

    auto* uiConfirm = uiContext->AddAction(InputMapper::Actions::UIConfirm, true);
    uiConfirm->AddBinding(InputBinding::MakeKey(Key::Enter));
    uiConfirm->AddBinding(InputBinding::MakeKey(Key::Space));

    auto* uiCancel = uiContext->AddAction(InputMapper::Actions::UICancel, true);
    uiCancel->AddBinding(InputBinding::MakeKey(Key::Escape));

    DebugInputListener debugListener;
    auto debugListenerId = inputManager.AddListener(debugListener, -100);

    windowInput.SetCursorLocked(true);
    windowInput.SetRawMouseMode(true);

    U32 lastWidth = window.GetWidth();
    U32 lastHeight = window.GetHeight();

    bool isPaused = false;
    bool showUI = false;

    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastFrameTime = startTime;

    Logger::Info("Input system initialized. Press ESC to toggle pause, F1 for UI");

    while (!window.ShouldClose()) {

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<F64>(currentTime - startTime).count();
        auto deltaTime = std::chrono::duration<F32>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        inputManager.BeginFrame(elapsed);
        window.PollEvents();

        inputMapper.Update(inputManager);

        if (window.GetWidth() != lastWidth || window.GetHeight() != lastHeight) {
            lastWidth = window.GetWidth();
            lastHeight = window.GetHeight();
            renderer.OnResize(lastWidth, lastHeight);
        }

        if (inputMapper.IsActionJustPressed(InputMapper::Actions::Pause)) {
            isPaused = !isPaused;
            windowInput.SetCursorLocked(!isPaused);
            windowInput.SetCursorVisible(isPaused);
            gameContext->SetActive(!isPaused);
            Logger::Info("Game {}", isPaused ? "paused" : "resumed");
        }

        if (inputManager.IsKeyJustPressed(Key::F1)) {
            showUI = !showUI;
            uiContext->SetActive(showUI);
            Logger::Info("UI {}", showUI ? "shown" : "hidden");
        }

        if (showUI) {
            if (inputMapper.IsActionJustPressed(InputMapper::Actions::UIConfirm)) {
                Logger::Info("UI Confirm pressed");
            }
            if (inputMapper.IsActionJustPressed(InputMapper::Actions::UICancel)) {
                showUI = false;
                uiContext->SetActive(false);
                Logger::Info("UI closed");
            }
        }

        if (inputManager.IsKeyJustPressed(Key::F3)) {
            const auto& state = inputManager.GetState();
            Logger::Debug("=== Input State Debug ===");
            Logger::Debug("Mouse Position: ({}, {})", state.mouseX, state.mouseY);
            Logger::Debug("Mouse Delta: ({}, {})", state.mouseDeltaX, state.mouseDeltaY);
            Logger::Debug("Active Modifiers: {}", static_cast<U8>(state.modifiers));

            std::string pressedKeys;
            for (USize i = 0; i <= static_cast<USize>(Key::Last); ++i) {
                if (state.keys[i]) {
                    if (!pressedKeys.empty()) pressedKeys += ", ";
                    pressedKeys += GetKeyName(static_cast<Key>(i));
                }
            }
            Logger::Debug("Pressed Keys: {}", pressedKeys.empty() ? "None" : pressedKeys);
        }

        if (inputManager.IsKeyJustPressed(Key::F12)) {
            Logger::Info("Screenshot requested (not implemented)");
        }

        inputManager.EndFrame();

        renderer.BeginFrame();
        pipeline.BeginFrame();
        pipeline.DrawMesh(*triangleMesh);
        renderer.EndFrame();
    }

    inputManager.RemoveListener(debugListenerId);

    Logger::Info("Shutting down...");

    return 0;
}