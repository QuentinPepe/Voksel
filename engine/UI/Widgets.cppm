export module UI.Widgets;

import UI.Core;
import UI.Element;
import Core.Types;
import Math.Vector;
import std;

export class UIPanel : public UIElement {
protected:
    Color m_BackgroundColor{0.2f, 0.2f, 0.2f, 0.9f};
    F32 m_BorderWidth{0.0f};
    Color m_BorderColor{0.4f, 0.4f, 0.4f, 1.0f};

public:
    void SetBackgroundColor(const Color& color) { m_BackgroundColor = color; }
    void SetBorderWidth(F32 width) { m_BorderWidth = width; }
    void SetBorderColor(const Color& color) { m_BorderColor = color; }

protected:
    void OnDraw(class UIRenderer* renderer) override;
};

export class UIText : public UIElement {
protected:
    std::string m_Text;
    U32 m_FontSize{14};
    Color m_TextColor{1.0f, 1.0f, 1.0f, 1.0f};
    Alignment m_HorizontalAlign{Alignment::Center};
    Alignment m_VerticalAlign{Alignment::Center};

public:
    void SetText(const std::string& text) { m_Text = text; }
    void SetFontSize(U32 size) { m_FontSize = size; }
    void SetTextColor(const Color& color) { m_TextColor = color; }
    void SetAlignment(Alignment horizontal, Alignment vertical) {
        m_HorizontalAlign = horizontal;
        m_VerticalAlign = vertical;
    }

    [[nodiscard]] const std::string& GetText() const { return m_Text; }

protected:
    void OnDraw(UIRenderer* renderer) override;
};

export class UIButton : public UIPanel {
protected:
    UIText* m_Label{nullptr};
    std::function<void()> m_OnClick;
    Color m_NormalColor{0.3f, 0.3f, 0.3f, 1.0f};
    Color m_HoverColor{0.4f, 0.4f, 0.4f, 1.0f};
    Color m_PressedColor{0.2f, 0.2f, 0.2f, 1.0f};
    bool m_IsHovered{false};
    bool m_IsPressed{false};

public:
    UIButton() {
        auto label = std::make_shared<UIText>();
        label->SetAnchor(AnchorPreset::StretchAll);
        label->SetMargin(Margin{5.0f});
        AddChild(label);
        m_Label = label.get();
        m_BackgroundColor = m_NormalColor;
    }

    void SetText(const std::string& text) {
        if (m_Label) m_Label->SetText(text);
    }

    void SetOnClick(std::function<void()> callback) {
        m_OnClick = std::move(callback);
    }

protected:
    void OnMouseDown(const Math::Vec2&) override {
        m_IsPressed = true;
        m_BackgroundColor = m_PressedColor;
    }

    void OnMouseUp(const Math::Vec2&) override {
        if (m_IsPressed && m_OnClick) {
            m_OnClick();
        }
        m_IsPressed = false;
        m_BackgroundColor = m_IsHovered ? m_HoverColor : m_NormalColor;
    }

    void OnMouseHover(const Math::Vec2&) override {
        if (!m_IsHovered) {
            m_IsHovered = true;
            if (!m_IsPressed) {
                m_BackgroundColor = m_HoverColor;
            }
        }
    }
};

export class UISlider : public UIPanel {
protected:
    F32 m_Value{0.5f};
    F32 m_MinValue{0.0f};
    F32 m_MaxValue{1.0f};
    UIPanel* m_Fill{nullptr};
    UIPanel* m_Handle{nullptr};
    std::function<void(F32)> m_OnValueChanged;
    bool m_IsDragging{false};

public:
    UISlider() {
        m_BackgroundColor = Color{0.2f, 0.2f, 0.2f, 1.0f};

        auto fill = std::make_shared<UIPanel>();
        fill->SetBackgroundColor(Color{0.3f, 0.6f, 0.9f, 1.0f});
        fill->SetAnchor(AnchorPreset::MiddleLeft);
        fill->SetPivot({0.0f, 0.5f});
        AddChild(fill);
        m_Fill = fill.get();

        auto handle = std::make_shared<UIPanel>();
        handle->SetBackgroundColor(Color{0.8f, 0.8f, 0.8f, 1.0f});
        handle->SetSizeDelta({20.0f, 20.0f});
        handle->SetAnchor(AnchorPreset::MiddleLeft);
        handle->SetPivot({0.5f, 0.5f});
        AddChild(handle);
        m_Handle = handle.get();

        UpdateVisuals();
    }

    void SetValue(F32 value) {
        m_Value = std::clamp(value, m_MinValue, m_MaxValue);
        UpdateVisuals();
        if (m_OnValueChanged) {
            m_OnValueChanged(m_Value);
        }
    }

    void SetRange(F32 min, F32 max) {
        m_MinValue = min;
        m_MaxValue = max;
        SetValue(m_Value);
    }

    void SetOnValueChanged(std::function<void(F32)> callback) {
        m_OnValueChanged = std::move(callback);
    }

    [[nodiscard]] F32 GetValue() const { return m_Value; }

protected:
    void OnMouseDown(const Math::Vec2& localPos) override {
        m_IsDragging = true;
        UpdateValueFromMouse(localPos);
    }

    void OnMouseUp(const Math::Vec2&) override {
        m_IsDragging = false;
    }

    void OnMouseHover(const Math::Vec2& localPos) override {
        if (m_IsDragging) {
            UpdateValueFromMouse(localPos);
        }
    }

    void UpdateValueFromMouse(const Math::Vec2& localPos) {
        F32 normalizedX = localPos.x / m_WorldRect.size.x;
        SetValue(m_MinValue + normalizedX * (m_MaxValue - m_MinValue));
    }

    void UpdateVisuals() {
        F32 normalized = (m_Value - m_MinValue) / (m_MaxValue - m_MinValue);
        F32 fillWidth = m_LocalRect.size.x * normalized;

        if (m_Fill) {
            m_Fill->SetSizeDelta({fillWidth, m_LocalRect.size.y});
        }

        if (m_Handle) {
            m_Handle->SetAnchoredPosition({fillWidth, 0.0f});
        }
    }
};

export class UIScrollView : public UIPanel {
protected:
    UIElement* m_Content{nullptr};
    UIPanel* m_VerticalScrollbar{nullptr};
    UIPanel* m_HorizontalScrollbar{nullptr};
    Math::Vec2 m_ScrollPosition{};
    Math::Vec2 m_ContentSize{};
    bool m_ShowVerticalScrollbar{true};
    bool m_ShowHorizontalScrollbar{false};

public:
    UIScrollView() {
        auto content = std::make_shared<UIElement>();
        content->SetAnchor(AnchorPreset::TopLeft);
        content->SetPivot({0.0f, 0.0f});
        AddChild(content);
        m_Content = content.get();

        if (m_ShowVerticalScrollbar) {
            auto vScrollbar = std::make_shared<UIPanel>();
            vScrollbar->SetBackgroundColor(Color{0.3f, 0.3f, 0.3f, 0.8f});
            vScrollbar->SetAnchor(AnchorPreset::StretchRight);
            vScrollbar->SetSizeDelta({15.0f, 0.0f});
            AddChild(vScrollbar);
            m_VerticalScrollbar = vScrollbar.get();
        }
    }

    void SetContentSize(const Math::Vec2& size) {
        m_ContentSize = size;
        if (m_Content) {
            m_Content->SetSizeDelta(size);
        }
        UpdateScrollbars();
    }

    void ScrollTo(const Math::Vec2& position) {
        m_ScrollPosition = position;
        UpdateContentPosition();
    }

    UIElement* GetContent() { return m_Content; }

protected:
    void UpdateContentPosition() {
        if (m_Content) {
            m_Content->SetAnchoredPosition(-m_ScrollPosition);
        }
    }

    void UpdateScrollbars() {
    }
};
