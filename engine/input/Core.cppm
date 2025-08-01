export module Input.Core;

import Core.Types;
import std;

export enum class Key : U16 {
    Unknown = static_cast<U16>(-1),

    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,

    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Semicolon = 59,
    Equal = 61,

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,

    Escape = 256,
    Enter, Tab, Backspace, Insert, Delete,
    Right, Left, Down, Up,
    PageUp, PageDown, Home, End,
    CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,

    Kp0 = 320, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd, KpEnter, KpEqual,

    LeftShift = 340, LeftControl, LeftAlt, LeftSuper,
    RightShift, RightControl, RightAlt, RightSuper,
    Menu,

    Last = Menu
};

export enum class MouseButton : U8 {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
    Last = Button8
};

export enum class InputEventAction : U8 {
    Release = 0,
    Press = 1,
    Repeat = 2
};

export enum class ModifierKey : U8 {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
    CapsLock = 1 << 4,
    NumLock = 1 << 5
};

export constexpr ModifierKey operator|(ModifierKey a, ModifierKey b) {
    return static_cast<ModifierKey>(static_cast<U8>(a) | static_cast<U8>(b));
}

export constexpr ModifierKey operator&(ModifierKey a, ModifierKey b) {
    return static_cast<ModifierKey>(static_cast<U8>(a) & static_cast<U8>(b));
}

export constexpr bool HasModifier(ModifierKey mods, ModifierKey mod) {
    return (mods & mod) != ModifierKey::None;
}

export struct KeyEvent {
    Key key;
    S32 scancode;
    InputEventAction action;
    ModifierKey mods;
    F64 timestamp;
};

export struct MouseButtonEvent {
    MouseButton button;
    InputEventAction action;
    ModifierKey mods;
    F64 x, y;
    F64 timestamp;
};

export struct MouseMoveEvent {
    F64 x, y;
    F64 dx, dy;
    F64 timestamp;
};

export struct MouseScrollEvent {
    F64 xOffset, yOffset;
    F64 x, y;
    F64 timestamp;
};

export struct TextInputEvent {
    U32 codepoint;
    F64 timestamp;
};

export struct InputState {

    Array<bool, static_cast<USize>(Key::Last) + 1> keys{};
    Array<F64, static_cast<USize>(Key::Last) + 1> keyPressTime{};

    Array<bool, static_cast<USize>(MouseButton::Last) + 1> mouseButtons{};
    Array<F64, static_cast<USize>(MouseButton::Last) + 1> mousePressTime{};
    F64 mouseX = 0.0, mouseY = 0.0;
    F64 mouseDeltaX = 0.0, mouseDeltaY = 0.0;
    F64 scrollX = 0.0, scrollY = 0.0;

    ModifierKey modifiers = ModifierKey::None;

    F64 currentTime = 0.0;
    F64 deltaTime = 0.0;

    void Reset() {
        keys.fill(false);
        keyPressTime.fill(0.0);
        mouseButtons.fill(false);
        mousePressTime.fill(0.0);
        mouseDeltaX = mouseDeltaY = 0.0;
        scrollX = scrollY = 0.0;
    }

    [[nodiscard]] bool IsKeyPressed(Key key) const {
        return keys[static_cast<USize>(key)];
    }

    [[nodiscard]] bool IsKeyJustPressed(Key key) const {
        return keys[static_cast<USize>(key)] &&
               (currentTime - keyPressTime[static_cast<USize>(key)]) < deltaTime;
    }

    [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const {
        return mouseButtons[static_cast<USize>(button)];
    }

    [[nodiscard]] bool IsMouseButtonJustPressed(MouseButton button) const {
        return mouseButtons[static_cast<USize>(button)] &&
               (currentTime - mousePressTime[static_cast<USize>(button)]) < deltaTime;
    }

    [[nodiscard]] bool HasModifier(ModifierKey mod) const {
        return (modifiers & mod) != ModifierKey::None;
    }
};

export const char* GetKeyName(Key key) {
    switch (key) {
        case Key::Space: return "Space";
        case Key::Apostrophe: return "'";
        case Key::Comma: return ",";
        case Key::Minus: return "-";
        case Key::Period: return ".";
        case Key::Slash: return "/";

        case Key::Num0: return "0";
        case Key::Num1: return "1";
        case Key::Num2: return "2";
        case Key::Num3: return "3";
        case Key::Num4: return "4";
        case Key::Num5: return "5";
        case Key::Num6: return "6";
        case Key::Num7: return "7";
        case Key::Num8: return "8";
        case Key::Num9: return "9";

        case Key::A: return "A";
        case Key::B: return "B";
        case Key::C: return "C";
        case Key::D: return "D";
        case Key::E: return "E";
        case Key::F: return "F";
        case Key::G: return "G";
        case Key::H: return "H";
        case Key::I: return "I";
        case Key::J: return "J";
        case Key::K: return "K";
        case Key::L: return "L";
        case Key::M: return "M";
        case Key::N: return "N";
        case Key::O: return "O";
        case Key::P: return "P";
        case Key::Q: return "Q";
        case Key::R: return "R";
        case Key::S: return "S";
        case Key::T: return "T";
        case Key::U: return "U";
        case Key::V: return "V";
        case Key::W: return "W";
        case Key::X: return "X";
        case Key::Y: return "Y";
        case Key::Z: return "Z";

        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";

        case Key::F1: return "F1";
        case Key::F2: return "F2";
        case Key::F3: return "F3";
        case Key::F4: return "F4";
        case Key::F5: return "F5";
        case Key::F6: return "F6";
        case Key::F7: return "F7";
        case Key::F8: return "F8";
        case Key::F9: return "F9";
        case Key::F10: return "F10";
        case Key::F11: return "F11";
        case Key::F12: return "F12";

        case Key::LeftShift: return "Left Shift";
        case Key::LeftControl: return "Left Control";
        case Key::LeftAlt: return "Left Alt";
        case Key::LeftSuper: return "Left Super";
        case Key::RightShift: return "Right Shift";
        case Key::RightControl: return "Right Control";
        case Key::RightAlt: return "Right Alt";
        case Key::RightSuper: return "Right Super";

        default: return "Unknown";
    }
}

export const char* GetMouseButtonName(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left Mouse";
        case MouseButton::Right: return "Right Mouse";
        case MouseButton::Middle: return "Middle Mouse";
        case MouseButton::Button4: return "Mouse Button 4";
        case MouseButton::Button5: return "Mouse Button 5";
        case MouseButton::Button6: return "Mouse Button 6";
        case MouseButton::Button7: return "Mouse Button 7";
        case MouseButton::Button8: return "Mouse Button 8";
        default: return "Unknown Mouse Button";
    }
}