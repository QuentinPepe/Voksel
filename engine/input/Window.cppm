module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

export module Input.Window;

import Input.Core;
import Input.Manager;
import Graphics.Window;
import Core.Types;
import Core.Log;
import std;

export class WindowInputHandler;

Key GLFWKeyToKey(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_SPACE: return Key::Space;
        case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
        case GLFW_KEY_COMMA: return Key::Comma;
        case GLFW_KEY_MINUS: return Key::Minus;
        case GLFW_KEY_PERIOD: return Key::Period;
        case GLFW_KEY_SLASH: return Key::Slash;

        case GLFW_KEY_0: return Key::Num0;
        case GLFW_KEY_1: return Key::Num1;
        case GLFW_KEY_2: return Key::Num2;
        case GLFW_KEY_3: return Key::Num3;
        case GLFW_KEY_4: return Key::Num4;
        case GLFW_KEY_5: return Key::Num5;
        case GLFW_KEY_6: return Key::Num6;
        case GLFW_KEY_7: return Key::Num7;
        case GLFW_KEY_8: return Key::Num8;
        case GLFW_KEY_9: return Key::Num9;

        case GLFW_KEY_SEMICOLON: return Key::Semicolon;
        case GLFW_KEY_EQUAL: return Key::Equal;

        case GLFW_KEY_A: return Key::A;
        case GLFW_KEY_B: return Key::B;
        case GLFW_KEY_C: return Key::C;
        case GLFW_KEY_D: return Key::D;
        case GLFW_KEY_E: return Key::E;
        case GLFW_KEY_F: return Key::F;
        case GLFW_KEY_G: return Key::G;
        case GLFW_KEY_H: return Key::H;
        case GLFW_KEY_I: return Key::I;
        case GLFW_KEY_J: return Key::J;
        case GLFW_KEY_K: return Key::K;
        case GLFW_KEY_L: return Key::L;
        case GLFW_KEY_M: return Key::M;
        case GLFW_KEY_N: return Key::N;
        case GLFW_KEY_O: return Key::O;
        case GLFW_KEY_P: return Key::P;
        case GLFW_KEY_Q: return Key::Q;
        case GLFW_KEY_R: return Key::R;
        case GLFW_KEY_S: return Key::S;
        case GLFW_KEY_T: return Key::T;
        case GLFW_KEY_U: return Key::U;
        case GLFW_KEY_V: return Key::V;
        case GLFW_KEY_W: return Key::W;
        case GLFW_KEY_X: return Key::X;
        case GLFW_KEY_Y: return Key::Y;
        case GLFW_KEY_Z: return Key::Z;

        case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
        case GLFW_KEY_BACKSLASH: return Key::Backslash;
        case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return Key::GraveAccent;

        case GLFW_KEY_ESCAPE: return Key::Escape;
        case GLFW_KEY_ENTER: return Key::Enter;
        case GLFW_KEY_TAB: return Key::Tab;
        case GLFW_KEY_BACKSPACE: return Key::Backspace;
        case GLFW_KEY_INSERT: return Key::Insert;
        case GLFW_KEY_DELETE: return Key::Delete;
        case GLFW_KEY_RIGHT: return Key::Right;
        case GLFW_KEY_LEFT: return Key::Left;
        case GLFW_KEY_DOWN: return Key::Down;
        case GLFW_KEY_UP: return Key::Up;
        case GLFW_KEY_PAGE_UP: return Key::PageUp;
        case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
        case GLFW_KEY_HOME: return Key::Home;
        case GLFW_KEY_END: return Key::End;
        case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
        case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
        case GLFW_KEY_NUM_LOCK: return Key::NumLock;
        case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
        case GLFW_KEY_PAUSE: return Key::Pause;

        case GLFW_KEY_F1: return Key::F1;
        case GLFW_KEY_F2: return Key::F2;
        case GLFW_KEY_F3: return Key::F3;
        case GLFW_KEY_F4: return Key::F4;
        case GLFW_KEY_F5: return Key::F5;
        case GLFW_KEY_F6: return Key::F6;
        case GLFW_KEY_F7: return Key::F7;
        case GLFW_KEY_F8: return Key::F8;
        case GLFW_KEY_F9: return Key::F9;
        case GLFW_KEY_F10: return Key::F10;
        case GLFW_KEY_F11: return Key::F11;
        case GLFW_KEY_F12: return Key::F12;

        case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
        case GLFW_KEY_LEFT_CONTROL: return Key::LeftControl;
        case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
        case GLFW_KEY_LEFT_SUPER: return Key::LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
        case GLFW_KEY_RIGHT_CONTROL: return Key::RightControl;
        case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
        case GLFW_KEY_RIGHT_SUPER: return Key::RightSuper;
        case GLFW_KEY_MENU: return Key::Menu;

        default: return Key::Unknown;
    }
}

MouseButton GLFWMouseButtonToButton(int glfwButton) {
    if (glfwButton >= 0 && glfwButton <= static_cast<int>(MouseButton::Last)) {
        return static_cast<MouseButton>(glfwButton);
    }
    return MouseButton::Left;
}

InputEventAction GLFWActionToAction(int glfwAction) {
    switch (glfwAction) {
        case GLFW_RELEASE: return InputEventAction::Release;
        case GLFW_PRESS: return InputEventAction::Press;
        case GLFW_REPEAT: return InputEventAction::Repeat;
        default: return InputEventAction::Release;
    }
}

ModifierKey GLFWModsToModifiers(int glfwMods) {
    ModifierKey mods = ModifierKey::None;

    if (glfwMods & GLFW_MOD_SHIFT) mods = mods | ModifierKey::Shift;
    if (glfwMods & GLFW_MOD_CONTROL) mods = mods | ModifierKey::Control;
    if (glfwMods & GLFW_MOD_ALT) mods = mods | ModifierKey::Alt;
    if (glfwMods & GLFW_MOD_SUPER) mods = mods | ModifierKey::Super;
    if (glfwMods & GLFW_MOD_CAPS_LOCK) mods = mods | ModifierKey::CapsLock;
    if (glfwMods & GLFW_MOD_NUM_LOCK) mods = mods | ModifierKey::NumLock;

    return mods;
}

struct WindowUserData {
    Window* window;
    WindowInputHandler* inputHandler;
};

export class WindowInputHandler {
private:
    Window* m_Window;
    InputManager* m_InputManager;
    UniquePtr<WindowUserData> m_UserData;

    bool m_CursorVisible = true;
    bool m_CursorLocked = false;
    F64 m_LastMouseX = 0.0;
    F64 m_LastMouseY = 0.0;
    bool m_FirstMouse = true;

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->inputHandler && userData->inputHandler->m_InputManager) {
            userData->inputHandler->m_InputManager->InjectKeyEvent(
                GLFWKeyToKey(key),
                scancode,
                GLFWActionToAction(action),
                GLFWModsToModifiers(mods)
            );
        }
    }

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->inputHandler && userData->inputHandler->m_InputManager) {
            userData->inputHandler->m_InputManager->InjectMouseButtonEvent(
                GLFWMouseButtonToButton(button),
                GLFWActionToAction(action),
                GLFWModsToModifiers(mods)
            );
        }
    }

    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->inputHandler && userData->inputHandler->m_InputManager) {
            auto* handler = userData->inputHandler;

            if (handler->m_FirstMouse) {
                handler->m_LastMouseX = xpos;
                handler->m_LastMouseY = ypos;
                handler->m_FirstMouse = false;
            }

            handler->m_InputManager->InjectMouseMoveEvent(xpos, ypos);

            handler->m_LastMouseX = xpos;
            handler->m_LastMouseY = ypos;
        }
    }

    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->inputHandler && userData->inputHandler->m_InputManager) {
            userData->inputHandler->m_InputManager->InjectMouseScrollEvent(xoffset, yoffset);
        }
    }

    static void CharCallback(GLFWwindow* window, unsigned int codepoint) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->inputHandler && userData->inputHandler->m_InputManager) {
            userData->inputHandler->m_InputManager->InjectTextInputEvent(codepoint);
        }
    }

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
        if (userData && userData->window) {

            userData->window->UpdateDimensions(width, height);
        }
    }

public:
    WindowInputHandler(Window* window, InputManager* inputManager)
        : m_Window{window}, m_InputManager{inputManager} {

        GLFWwindow* glfwWindow = m_Window->GetGLFWHandle();

        m_UserData = std::make_unique<WindowUserData>();
        m_UserData->window = m_Window;
        m_UserData->inputHandler = this;

        glfwSetWindowUserPointer(glfwWindow, m_UserData.get());

        glfwSetKeyCallback(glfwWindow, KeyCallback);
        glfwSetMouseButtonCallback(glfwWindow, MouseButtonCallback);
        glfwSetCursorPosCallback(glfwWindow, CursorPosCallback);
        glfwSetScrollCallback(glfwWindow, ScrollCallback);
        glfwSetCharCallback(glfwWindow, CharCallback);
        glfwSetFramebufferSizeCallback(glfwWindow, FramebufferSizeCallback);

        glfwGetCursorPos(glfwWindow, &m_LastMouseX, &m_LastMouseY);

        Logger::Info("Window input handler initialized");
    }

    ~WindowInputHandler() {
        GLFWwindow* glfwWindow = m_Window->GetGLFWHandle();

        glfwSetKeyCallback(glfwWindow, nullptr);
        glfwSetMouseButtonCallback(glfwWindow, nullptr);
        glfwSetCursorPosCallback(glfwWindow, nullptr);
        glfwSetScrollCallback(glfwWindow, nullptr);
        glfwSetCharCallback(glfwWindow, nullptr);
        glfwSetFramebufferSizeCallback(glfwWindow, nullptr);

        glfwSetWindowUserPointer(glfwWindow, nullptr);
    }

    void SetCursorVisible(bool visible) {
        if (m_CursorVisible != visible) {
            m_CursorVisible = visible;
            UpdateCursorMode();
        }
    }

    void SetCursorLocked(bool locked) {
        if (m_CursorLocked != locked) {
            m_CursorLocked = locked;
            UpdateCursorMode();

            if (locked) {
                m_FirstMouse = true;
            }
        }
    }

    [[nodiscard]] bool IsCursorVisible() const { return m_CursorVisible; }
    [[nodiscard]] bool IsCursorLocked() const { return m_CursorLocked; }

    void SetRawMouseMode(bool raw) const {
        GLFWwindow* glfwWindow = m_Window->GetGLFWHandle();

        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(glfwWindow, GLFW_RAW_MOUSE_MOTION, raw ? GLFW_TRUE : GLFW_FALSE);
            m_InputManager->SetRawMouseMode(raw);
            Logger::Debug("Raw mouse mode {}", raw ? "enabled" : "disabled");
        } else {
            Logger::Warn("Raw mouse motion not supported");
        }
    }

private:
    void UpdateCursorMode() const {
        GLFWwindow* glfwWindow = m_Window->GetGLFWHandle();

        int mode = GLFW_CURSOR_NORMAL;
        if (!m_CursorVisible) {
            mode = m_CursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_HIDDEN;
        }

        glfwSetInputMode(glfwWindow, GLFW_CURSOR, mode);
        m_InputManager->SetMouseCaptured(m_CursorLocked);
    }
};