export module Input.Bindings;

import Input.Core;
import Input.Manager;
import Core.Types;
import Core.Log;
import std;

export struct InputBinding {
    enum class Type : U8 {
        Key,
        MouseButton,
        MouseAxis,
        Composite
    } type;

    union {
        struct {
            Key key;
            ModifierKey requiredMods;
        } key;

        struct {
            MouseButton button;
            ModifierKey requiredMods;
        } mouseButton;

        struct {
            enum Axis : U8 { X, Y, ScrollX, ScrollY } axis;
            F32 scale;
            F32 deadzone;
        } mouseAxis;

        struct {
            Key positive;
            Key negative;
            ModifierKey requiredMods;
        } composite;
    };

    static InputBinding MakeKey(Key k, ModifierKey mods = ModifierKey::None) {
        InputBinding binding{};
        binding.type = Type::Key;
        binding.key.key = k;
        binding.key.requiredMods = mods;
        return binding;
    }

    static InputBinding MakeMouseButton(MouseButton btn, ModifierKey mods = ModifierKey::None) {
        InputBinding binding{};
        binding.type = Type::MouseButton;
        binding.mouseButton.button = btn;
        binding.mouseButton.requiredMods = mods;
        return binding;
    }

    static InputBinding MakeMouseAxis(U8 axis, F32 scale = 1.0f, F32 deadzone = 0.0f) {
        InputBinding binding{};
        binding.type = Type::MouseAxis;
        binding.mouseAxis.axis = static_cast<decltype(mouseAxis.axis)>(axis);
        binding.mouseAxis.scale = scale;
        binding.mouseAxis.deadzone = deadzone;
        return binding;
    }

    static InputBinding MakeComposite(Key positive, Key negative, ModifierKey mods = ModifierKey::None) {
        InputBinding binding{};
        binding.type = Type::Composite;
        binding.composite.positive = positive;
        binding.composite.negative = negative;
        binding.composite.requiredMods = mods;
        return binding;
    }
};

export class InputAction {
private:
    std::string m_Name;
    Vector<InputBinding> m_Bindings;
    F32 m_Value = 0.0f;
    F32 m_PreviousValue = 0.0f;
    bool m_ConsumeInput = false;

public:
    explicit InputAction(std::string name, bool consumeInput = false)
        : m_Name{std::move(name)}, m_ConsumeInput{consumeInput} {}

    void AddBinding(const InputBinding& binding) {
        m_Bindings.push_back(binding);
    }

    void Update(const InputManager& input) {
        m_PreviousValue = m_Value;
        m_Value = 0.0f;

        const auto& state = input.GetState();

        for (const auto& binding : m_Bindings) {
            F32 bindingValue = 0.0f;

            switch (binding.type) {
                case InputBinding::Type::Key:
                    if (state.modifiers == binding.key.requiredMods &&
                        state.IsKeyPressed(binding.key.key)) {
                        bindingValue = 1.0f;
                    }
                    break;

                case InputBinding::Type::MouseButton:
                    if (state.modifiers == binding.mouseButton.requiredMods &&
                        state.IsMouseButtonPressed(binding.mouseButton.button)) {
                        bindingValue = 1.0f;
                    }
                    break;

                case InputBinding::Type::MouseAxis: {
                    F32 axisValue = 0.0f;
                    switch (binding.mouseAxis.axis) {
                        case 0: axisValue = static_cast<F32>(state.mouseDeltaX); break;
                        case 1: axisValue = static_cast<F32>(state.mouseDeltaY); break;
                        case 2: axisValue = static_cast<F32>(state.scrollX); break;
                        case 3: axisValue = static_cast<F32>(state.scrollY); break;
                    }

                    if (std::abs(axisValue) > binding.mouseAxis.deadzone) {
                        bindingValue = axisValue * binding.mouseAxis.scale;
                    }
                    break;
                }

                case InputBinding::Type::Composite:
                    if (state.modifiers == binding.composite.requiredMods) {
                        if (state.IsKeyPressed(binding.composite.positive)) {
                            bindingValue += 1.0f;
                        }
                        if (state.IsKeyPressed(binding.composite.negative)) {
                            bindingValue -= 1.0f;
                        }
                    }
                    break;
            }

            // Take the binding with the highest absolute value
            if (std::abs(bindingValue) > std::abs(m_Value)) {
                m_Value = bindingValue;
            }
        }
    }

    [[nodiscard]] F32 GetValue() const { return m_Value; }
    [[nodiscard]] F32 GetPreviousValue() const { return m_PreviousValue; }
    [[nodiscard]] bool IsPressed() const { return m_Value != 0.0f; }
    [[nodiscard]] bool WasPressed() const { return m_PreviousValue != 0.0f; }
    [[nodiscard]] bool JustPressed() const { return IsPressed() && !WasPressed(); }
    [[nodiscard]] bool JustReleased() const { return !IsPressed() && WasPressed(); }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] bool ShouldConsumeInput() const { return m_ConsumeInput; }
};

export class InputContext : public std::enable_shared_from_this<InputContext> {
private:
    std::string m_Name;
    UnorderedMap<std::string, UniquePtr<InputAction>> m_Actions;
    bool m_Active = true;
    S32 m_Priority = 0;

public:
    explicit InputContext(std::string name, S32 priority = 0)
        : m_Name{std::move(name)}, m_Priority{priority} {}

    InputAction* AddAction(const std::string& name, bool consumeInput = false) {
        auto action = std::make_unique<InputAction>(name, consumeInput);
        auto* ptr = action.get();
        m_Actions[name] = std::move(action);
        return ptr;
    }

    void RemoveAction(const std::string& name) {
        m_Actions.erase(name);
    }

    [[nodiscard]] InputAction* GetAction(const std::string& name) {
        auto it = m_Actions.find(name);
        return it != m_Actions.end() ? it->second.get() : nullptr;
    }

    void Update(const InputManager& input) {
        if (!m_Active) return;

        for (auto& [name, action] : m_Actions) {
            action->Update(input);
        }
    }

    void SetActive(bool active) { m_Active = active; }
    [[nodiscard]] bool IsActive() const { return m_Active; }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] S32 GetPriority() const { return m_Priority; }
};

export class InputMapper {
private:
    Vector<std::shared_ptr<InputContext>> m_Contexts;
    bool m_ContextsDirty = true;

    // Predefined action names
    struct ActionNames {
        // Movement
        static constexpr const char* MoveForward = "MoveForward";
        static constexpr const char* MoveBackward = "MoveBackward";
        static constexpr const char* MoveLeft = "MoveLeft";
        static constexpr const char* MoveRight = "MoveRight";
        static constexpr const char* MoveUp = "MoveUp";
        static constexpr const char* MoveDown = "MoveDown";

        // Camera
        static constexpr const char* LookX = "LookX";
        static constexpr const char* LookY = "LookY";
        static constexpr const char* Zoom = "Zoom";

        // Actions
        static constexpr const char* Jump = "Jump";
        static constexpr const char* Sprint = "Sprint";
        static constexpr const char* Crouch = "Crouch";
        static constexpr const char* Interact = "Interact";
        static constexpr const char* Fire = "Fire";
        static constexpr const char* AltFire = "AltFire";

        // UI
        static constexpr const char* UIConfirm = "UIConfirm";
        static constexpr const char* UICancel = "UICancel";
        static constexpr const char* UINavigateUp = "UINavigateUp";
        static constexpr const char* UINavigateDown = "UINavigateDown";
        static constexpr const char* UINavigateLeft = "UINavigateLeft";
        static constexpr const char* UINavigateRight = "UINavigateRight";

        // System
        static constexpr const char* Pause = "Pause";
        static constexpr const char* Console = "Console";
        static constexpr const char* Screenshot = "Screenshot";
    };

public:
    using Actions = ActionNames;

    std::shared_ptr<InputContext> CreateContext(const std::string& name, S32 priority = 0) {
        auto context = std::make_shared<InputContext>(name, priority);
        m_Contexts.push_back(context);
        m_ContextsDirty = true;
        return context;
    }

    void RemoveContext(const std::shared_ptr<InputContext>& context) {
        auto it = std::find(m_Contexts.begin(), m_Contexts.end(), context);
        if (it != m_Contexts.end()) {
            m_Contexts.erase(it);
            m_ContextsDirty = true;
        }
    }

    void Update(const InputManager& input) {
        SortContexts();

        for (auto& context : m_Contexts) {
            context->Update(input);
        }
    }

    [[nodiscard]] F32 GetActionValue(const std::string& actionName) const {
        for (const auto& context : m_Contexts) {
            if (!context->IsActive()) continue;

            if (auto* action = context->GetAction(actionName)) {
                if (action->GetValue() != 0.0f) {
                    return action->GetValue();
                }
            }
        }
        return 0.0f;
    }

    [[nodiscard]] bool IsActionPressed(const std::string& actionName) const {
        return GetActionValue(actionName) != 0.0f;
    }

    [[nodiscard]] bool IsActionJustPressed(const std::string& actionName) const {
        for (const auto& context : m_Contexts) {
            if (!context->IsActive()) continue;

            if (auto* action = context->GetAction(actionName)) {
                if (action->JustPressed()) {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool IsActionJustReleased(const std::string& actionName) const {
        using namespace std::ranges;
        return any_of(m_Contexts, [&](const auto& context) {
            if (!context->IsActive()) return false;
            if (auto* action = context->GetAction(actionName)) {
                return action->JustReleased();
            }
            return false;
        });
    }

    void SetupFPSControls(std::shared_ptr<InputContext> context) {
        // Movement
        auto* moveForward = context->AddAction(Actions::MoveForward);
        moveForward->AddBinding(InputBinding::MakeKey(Key::W));
        moveForward->AddBinding(InputBinding::MakeKey(Key::Up));

        auto* moveBackward = context->AddAction(Actions::MoveBackward);
        moveBackward->AddBinding(InputBinding::MakeKey(Key::S));
        moveBackward->AddBinding(InputBinding::MakeKey(Key::Down));

        auto* moveLeft = context->AddAction(Actions::MoveLeft);
        moveLeft->AddBinding(InputBinding::MakeKey(Key::A));
        moveLeft->AddBinding(InputBinding::MakeKey(Key::Left));

        auto* moveRight = context->AddAction(Actions::MoveRight);
        moveRight->AddBinding(InputBinding::MakeKey(Key::D));
        moveRight->AddBinding(InputBinding::MakeKey(Key::Right));

        // Vertical movement
        auto* moveUp = context->AddAction(Actions::MoveUp);
        moveUp->AddBinding(InputBinding::MakeKey(Key::Space));

        auto* moveDown = context->AddAction(Actions::MoveDown);
        moveDown->AddBinding(InputBinding::MakeKey(Key::LeftControl));

        // Look
        auto* lookX = context->AddAction(Actions::LookX);
        lookX->AddBinding(InputBinding::MakeMouseAxis(0, 0.1f));

        auto* lookY = context->AddAction(Actions::LookY);
        lookY->AddBinding(InputBinding::MakeMouseAxis(1, 0.1f));

        // Actions
        auto* jump = context->AddAction(Actions::Jump);
        jump->AddBinding(InputBinding::MakeKey(Key::Space));

        auto* sprint = context->AddAction(Actions::Sprint);
        sprint->AddBinding(InputBinding::MakeKey(Key::LeftShift));

        auto* crouch = context->AddAction(Actions::Crouch);
        crouch->AddBinding(InputBinding::MakeKey(Key::LeftControl));

        auto* interact = context->AddAction(Actions::Interact);
        interact->AddBinding(InputBinding::MakeKey(Key::E));

        auto* fire = context->AddAction(Actions::Fire);
        fire->AddBinding(InputBinding::MakeMouseButton(MouseButton::Left));

        auto* altFire = context->AddAction(Actions::AltFire);
        altFire->AddBinding(InputBinding::MakeMouseButton(MouseButton::Right));

        // System
        auto* pause = context->AddAction(Actions::Pause, true);
        pause->AddBinding(InputBinding::MakeKey(Key::Escape));
    }

private:
    void SortContexts() {
        if (!m_ContextsDirty) return;

        std::ranges::sort(m_Contexts,
                          [](const auto& a, const auto& b) {
                              return a->GetPriority() > b->GetPriority();
                          });

        m_ContextsDirty = false;
    }
};