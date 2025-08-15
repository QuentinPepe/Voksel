export module UI.Manager;

import UI.Core;
import UI.Element;
import UI.Layout;
import UI.Widgets;
import UI.Renderer;
import Graphics;
import Input.Core;
import Input.Manager;
import Math.Vector;
import Core.Types;
import std;

export class UIManager {
private:
    UIElementPtr m_Root;
    UIRenderer* m_Renderer{nullptr};
    InputManager* m_InputManager{nullptr};
    Math::Vec2 m_ScreenSize{1280.0f, 720.0f};
    bool m_MouseWasDown{false};

public:
    UIManager(IGraphicsContext* graphics, InputManager* input)
        : m_InputManager{input} {
        m_Renderer = new UIRenderer(graphics);
        m_Root = std::make_shared<UIElement>();
        m_Root->SetName("Root");
        m_Root->SetAnchor(AnchorPreset::StretchAll);
        m_Root->SetPivot({0.0f, 0.0f});
        m_Root->SetAnchoredPosition({0.0f, 0.0f});
        m_Root->SetSizeDelta(m_ScreenSize);
    }

    ~UIManager() {
        delete m_Renderer;
    }

    void SetScreenSize(F32 width, F32 height) {
        m_ScreenSize = {width, height};
        m_Root->SetSizeDelta(m_ScreenSize);
        m_Renderer->SetScreenSize(width, height);
    }

    void Update(F32 deltaTime) {
        m_Root->UpdateLayout();
        if (m_InputManager) {
            auto [mouseX, mouseY] = m_InputManager->GetMousePosition();
            Math::Vec2 mousePos{static_cast<F32>(mouseX), static_cast<F32>(mouseY)};
            bool mouseDown = m_InputManager->IsMouseButtonJustPressed(MouseButton::Left);
            bool mouseUp = m_InputManager->IsMouseButtonJustReleased(MouseButton::Left);
            m_Root->HandleInput(mousePos, mouseDown, mouseUp);
        }
    }

    void Render() {
        m_Renderer->BeginFrame();
        m_Root->Draw(m_Renderer);
        m_Renderer->EndFrame();
    }

    UIElementPtr GetRoot() { return m_Root; }

    UIElementPtr CreatePanel(const std::string& name = "") {
        auto panel = std::make_shared<UIPanel>();
        panel->SetName(name);
        return panel;
    }

    UIElementPtr CreateButton(const std::string& text = "Button") {
        auto button = std::make_shared<UIButton>();
        button->SetText(text);
        return button;
    }

    UIElementPtr CreateText(const std::string& text = "") {
        auto textElement = std::make_shared<UIText>();
        textElement->SetText(text);
        return textElement;
    }

    UIElementPtr CreateSlider(F32 min = 0.0f, F32 max = 1.0f, F32 value = 0.5f) {
        auto slider = std::make_shared<UISlider>();
        slider->SetRange(min, max);
        slider->SetValue(value);
        return slider;
    }

    UIElementPtr CreateVerticalLayout() {
        return std::make_shared<UIVerticalLayout>();
    }

    UIElementPtr CreateHorizontalLayout() {
        return std::make_shared<UIHorizontalLayout>();
    }

    UIElementPtr CreateGridLayout(U32 columns = 2) {
        auto grid = std::make_shared<UIGridLayout>();
        grid->SetColumns(columns);
        return grid;
    }

    UIElementPtr CreateScrollView() {
        return std::make_shared<UIScrollView>();
    }

    UIElementPtr FindElement(const std::string& name) {
        return m_Root->FindChildRecursive(name);
    }
};
