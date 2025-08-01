export module Input.Manager;

import Input.Core;
import Core.Types;
import Core.Log;
import std;

export template<typename T>
concept InputListener = requires(T t, const KeyEvent& ke, const MouseButtonEvent& mbe,
                                const MouseMoveEvent& mme, const MouseScrollEvent& mse,
                                const TextInputEvent& tie) {
    { t.OnKeyEvent(ke) } -> std::same_as<void>;
    { t.OnMouseButtonEvent(mbe) } -> std::same_as<void>;
    { t.OnMouseMoveEvent(mme) } -> std::same_as<void>;
    { t.OnMouseScrollEvent(mse) } -> std::same_as<void>;
    { t.OnTextInputEvent(tie) } -> std::same_as<void>;
};

export class InputManager {
private:
    struct ListenerEntry {
        std::function<void(const KeyEvent&)> onKey;
        std::function<void(const MouseButtonEvent&)> onMouseButton;
        std::function<void(const MouseMoveEvent&)> onMouseMove;
        std::function<void(const MouseScrollEvent&)> onMouseScroll;
        std::function<void(const TextInputEvent&)> onTextInput;
        S32 priority;
        bool blockEvents;
    };

    InputState m_CurrentState;
    InputState m_PreviousState;

    Vector<KeyEvent> m_KeyEvents;
    Vector<MouseButtonEvent> m_MouseButtonEvents;
    Vector<MouseMoveEvent> m_MouseMoveEvents;
    Vector<MouseScrollEvent> m_MouseScrollEvents;
    Vector<TextInputEvent> m_TextInputEvents;

    UnorderedMap<USize, ListenerEntry> m_Listeners;
    USize m_NextListenerId = 0;
    Vector<std::pair<S32, USize>> m_SortedListeners;
    bool m_ListenersDirty = true;

    bool m_MouseCaptured = false;
    bool m_RawMouseMode = false;

    bool m_TextInputActive = false;

    F64 m_LastFrameTime = 0.0;

public:
    InputManager() = default;

    void BeginFrame(F64 currentTime) {

        m_PreviousState = m_CurrentState;

        m_CurrentState.deltaTime = currentTime - m_LastFrameTime;
        m_CurrentState.currentTime = currentTime;
        m_LastFrameTime = currentTime;

        m_CurrentState.mouseDeltaX = 0.0;
        m_CurrentState.mouseDeltaY = 0.0;
        m_CurrentState.scrollX = 0.0;
        m_CurrentState.scrollY = 0.0;

        m_KeyEvents.clear();
        m_MouseButtonEvents.clear();
        m_MouseMoveEvents.clear();
        m_MouseScrollEvents.clear();
        m_TextInputEvents.clear();
    }

    void ProcessEvents() {
        UpdateSortedListeners();

        for (const auto& event : m_KeyEvents) {
            if (event.action == InputEventAction::Press) {
                m_CurrentState.keys[static_cast<USize>(event.key)] = true;
                m_CurrentState.keyPressTime[static_cast<USize>(event.key)] = event.timestamp;
            } else if (event.action == InputEventAction::Release) {
                m_CurrentState.keys[static_cast<USize>(event.key)] = false;
            }

            m_CurrentState.modifiers = event.mods;

            if (!DispatchEvent(event, &ListenerEntry::onKey)) break;
        }

        for (const auto& event : m_MouseButtonEvents) {
            if (event.action == InputEventAction::Press) {
                m_CurrentState.mouseButtons[static_cast<USize>(event.button)] = true;
                m_CurrentState.mousePressTime[static_cast<USize>(event.button)] = event.timestamp;
            } else if (event.action == InputEventAction::Release) {
                m_CurrentState.mouseButtons[static_cast<USize>(event.button)] = false;
            }

            m_CurrentState.modifiers = event.mods;

            if (!DispatchEvent(event, &ListenerEntry::onMouseButton)) break;
        }

        for (const auto& event : m_MouseMoveEvents) {
            if (!DispatchEvent(event, &ListenerEntry::onMouseMove)) break;
        }

        for (const auto& event : m_MouseScrollEvents) {
            if (!DispatchEvent(event, &ListenerEntry::onMouseScroll)) break;
        }

        for (const auto& event : m_TextInputEvents) {
            if (!DispatchEvent(event, &ListenerEntry::onTextInput)) break;
        }
    }

    void EndFrame() {
        Logger::Trace("InputManager::EndFrame - {} key events processed", m_KeyEvents.size());
    }

    void InjectKeyEvent(Key key, S32 scancode, InputEventAction action, ModifierKey mods) {
        m_KeyEvents.push_back({key, scancode, action, mods, m_CurrentState.currentTime});
    }

    void InjectMouseButtonEvent(MouseButton button, InputEventAction action, ModifierKey mods) {
        m_MouseButtonEvents.push_back({
            button, action, mods,
            m_CurrentState.mouseX, m_CurrentState.mouseY,
            m_CurrentState.currentTime
        });
    }

    void InjectMouseMoveEvent(F64 x, F64 y) {
        F64 dx = x - m_CurrentState.mouseX;
        F64 dy = y - m_CurrentState.mouseY;

        m_MouseMoveEvents.push_back({x, y, dx, dy, m_CurrentState.currentTime});

        m_CurrentState.mouseX = x;
        m_CurrentState.mouseY = y;
        m_CurrentState.mouseDeltaX += dx;
        m_CurrentState.mouseDeltaY += dy;
    }

    void InjectMouseScrollEvent(F64 xOffset, F64 yOffset) {
        m_MouseScrollEvents.push_back({
            xOffset, yOffset,
            m_CurrentState.mouseX, m_CurrentState.mouseY,
            m_CurrentState.currentTime
        });

        m_CurrentState.scrollX += xOffset;
        m_CurrentState.scrollY += yOffset;
    }

    void InjectTextInputEvent(U32 codepoint) {
        if (m_TextInputActive) {
            m_TextInputEvents.push_back({codepoint, m_CurrentState.currentTime});
        }
    }

    template<InputListener T>
    USize AddListener(T& listener, S32 priority = 0, bool blockEvents = false) {
        USize id = m_NextListenerId++;

        ListenerEntry entry{
            [&listener](const KeyEvent& e) { listener.OnKeyEvent(e); },
            [&listener](const MouseButtonEvent& e) { listener.OnMouseButtonEvent(e); },
            [&listener](const MouseMoveEvent& e) { listener.OnMouseMoveEvent(e); },
            [&listener](const MouseScrollEvent& e) { listener.OnMouseScrollEvent(e); },
            [&listener](const TextInputEvent& e) { listener.OnTextInputEvent(e); },
            priority,
            blockEvents
        };

        m_Listeners[id] = std::move(entry);
        m_ListenersDirty = true;

        return id;
    }

    void RemoveListener(USize id) {
        m_Listeners.erase(id);
        m_ListenersDirty = true;
    }

    [[nodiscard]] const InputState& GetState() const { return m_CurrentState; }
    [[nodiscard]] const InputState& GetPreviousState() const { return m_PreviousState; }

    [[nodiscard]] bool IsKeyPressed(Key key) const {
        return m_CurrentState.IsKeyPressed(key);
    }

    [[nodiscard]] bool IsKeyJustPressed(Key key) const {
        bool isPressed = m_CurrentState.IsKeyPressed(key);
        bool wasPressed = m_PreviousState.IsKeyPressed(key);
        bool result = isPressed && !wasPressed;

        return result;
    }

    [[nodiscard]] bool IsKeyJustReleased(Key key) const {
        return !m_CurrentState.IsKeyPressed(key) && m_PreviousState.IsKeyPressed(key);
    }

    [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const {
        return m_CurrentState.IsMouseButtonPressed(button);
    }

    [[nodiscard]] bool IsMouseButtonJustPressed(MouseButton button) const {
        return m_CurrentState.IsMouseButtonPressed(button) &&
               !m_PreviousState.IsMouseButtonPressed(button);
    }

    [[nodiscard]] bool IsMouseButtonJustReleased(MouseButton button) const {
        return !m_CurrentState.IsMouseButtonPressed(button) &&
               m_PreviousState.IsMouseButtonPressed(button);
    }

    [[nodiscard]] std::pair<F64, F64> GetMousePosition() const {
        return {m_CurrentState.mouseX, m_CurrentState.mouseY};
    }

    [[nodiscard]] std::pair<F64, F64> GetMouseDelta() const {
        return {m_CurrentState.mouseDeltaX, m_CurrentState.mouseDeltaY};
    }

    [[nodiscard]] std::pair<F64, F64> GetMouseScroll() const {
        return {m_CurrentState.scrollX, m_CurrentState.scrollY};
    }

    [[nodiscard]] bool HasModifier(ModifierKey mod) const {
        return m_CurrentState.HasModifier(mod);
    }

    void SetMouseCaptured(bool captured) { m_MouseCaptured = captured; }
    [[nodiscard]] bool IsMouseCaptured() const { return m_MouseCaptured; }

    void SetRawMouseMode(bool raw) { m_RawMouseMode = raw; }
    [[nodiscard]] bool IsRawMouseMode() const { return m_RawMouseMode; }

    void SetTextInputActive(bool active) { m_TextInputActive = active; }
    [[nodiscard]] bool IsTextInputActive() const { return m_TextInputActive; }

    [[nodiscard]] bool IsKeyCombo(Key key, ModifierKey requiredMods) const {
        return IsKeyJustPressed(key) && m_CurrentState.modifiers == requiredMods;
    }

    [[nodiscard]] bool IsAnyKeyPressed() const {
        for (USize i = 0; i <= static_cast<USize>(Key::Last); ++i) {
            if (m_CurrentState.keys[i]) return true;
        }
        return false;
    }

    [[nodiscard]] bool IsAnyMouseButtonPressed() const {
        for (USize i = 0; i <= static_cast<USize>(MouseButton::Last); ++i) {
            if (m_CurrentState.mouseButtons[i]) return true;
        }
        return false;
    }

private:
    template<typename EventType>
    bool DispatchEvent(const EventType& event,
                      std::function<void(const EventType&)> ListenerEntry::* memberPtr) {
        for (const auto &id: m_SortedListeners | std::views::values) {
            if (auto it = m_Listeners.find(id); it != m_Listeners.end()) {
                (it->second.*memberPtr)(event);
                if (it->second.blockEvents) {
                    return false;
                }
            }
        }
        return true;
    }

    void UpdateSortedListeners() {
        if (!m_ListenersDirty) return;

        m_SortedListeners.clear();
        for (const auto& [id, entry] : m_Listeners) {
            m_SortedListeners.emplace_back(entry.priority, id);
        }

        std::ranges::sort(m_SortedListeners,
                          [](const auto& a, const auto& b) { return a.first > b.first; });

        m_ListenersDirty = false;
    }
};