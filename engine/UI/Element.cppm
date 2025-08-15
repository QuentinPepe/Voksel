export module UI.Element;

import UI.Core;
import Core.Types;
import Core.Assert;
import Math.Vector;
import std;

export class UIElement;
export using UIElementPtr = std::shared_ptr<UIElement>;
export using UIElementWeakPtr = std::weak_ptr<UIElement>;

export class UIElement : public std::enable_shared_from_this<UIElement> {
protected:
    UIElementWeakPtr m_Parent;
    Vector<UIElementPtr> m_Children;

    std::string m_Name;
    U32 m_Id{0};

    Anchor m_Anchor{0.5f, 0.5f};
    Math::Vec2 m_AnchoredPosition{};
    Math::Vec2 m_SizeDelta{100.0f, 100.0f};
    Math::Vec2 m_Pivot{0.5f, 0.5f};
    F32 m_Rotation{0.0f};
    Math::Vec2 m_Scale{1.0f, 1.0f};

    Margin m_Margin{};

    Rect m_LocalRect{};
    Rect m_WorldRect{};

    Visibility m_Visibility{Visibility::Visible};
    bool m_Interactable{true};
    bool m_DirtyLayout{true};
    bool m_DirtyTransform{true};

    static inline U32 s_NextId{1};

public:
    UIElement() : m_Id{s_NextId++} {}
    virtual ~UIElement() = default;

    void AddChild(UIElementPtr child) {
        if (child && child.get() != this) {
            if (auto oldParent{child->m_Parent.lock()}) {
                oldParent->RemoveChild(child);
            }
            m_Children.push_back(child);
            child->m_Parent = weak_from_this();
            child->MarkDirty();
            MarkDirty();
        }
    }

    void RemoveChild(UIElementPtr child) {
        auto it = std::find(m_Children.begin(), m_Children.end(), child);
        if (it != m_Children.end()) {
            (*it)->m_Parent.reset();
            m_Children.erase(it);
            MarkDirty();
        }
    }

    void RemoveAllChildren() {
        for (auto& child : m_Children) {
            child->m_Parent.reset();
        }
        m_Children.clear();
        MarkDirty();
    }

    UIElementPtr GetChild(USize index) const {
        return index < m_Children.size() ? m_Children[index] : nullptr;
    }

    UIElementPtr FindChild(const std::string& name) const {
        for (const auto& child : m_Children) {
            if (child->m_Name == name) return child;
        }
        return nullptr;
    }

    UIElementPtr FindChildRecursive(const std::string& name) const {
        for (const auto& child : m_Children) {
            if (child->m_Name == name) return child;
            if (auto found = child->FindChildRecursive(name)) return found;
        }
        return nullptr;
    }

    virtual void UpdateLayout() {
        if (!m_DirtyLayout && !m_DirtyTransform) return;

        CalculateLocalRect();
        CalculateWorldRect();

        m_DirtyLayout = false;
        m_DirtyTransform = false;

        for (auto& child : m_Children) {
            child->UpdateLayout();
        }
    }

    virtual void Draw(class UIRenderer* renderer) {
        if (m_Visibility == Visibility::Visible) {
            OnDraw(renderer);
            for (const auto& child : m_Children) {
                child->Draw(renderer);
            }
        }
    }

    virtual bool HandleInput(const Math::Vec2& mousePos, bool mouseDown, bool mouseUp) {
        if (m_Visibility != Visibility::Visible || !m_Interactable) return false;

        for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
            if ((*it)->HandleInput(mousePos, mouseDown, mouseUp)) return true;
        }

        if (m_WorldRect.Contains(mousePos)) {
            if (mouseDown) OnMouseDown(mousePos);
            if (mouseUp) OnMouseUp(mousePos);
            OnMouseHover(mousePos);
            return true;
        }

        return false;
    }

    void SetAnchor(AnchorPreset preset) {
        m_Anchor = Anchor::FromPreset(preset);
        MarkDirty();
    }

    void SetAnchor(const Anchor& anchor) {
        m_Anchor = anchor;
        MarkDirty();
    }

    void SetAnchoredPosition(const Math::Vec2& pos) {
        m_AnchoredPosition = pos;
        MarkDirty();
    }

    void SetSizeDelta(const Math::Vec2& size) {
        m_SizeDelta = size;
        MarkDirty();
    }

    void SetPivot(const Math::Vec2& pivot) {
        m_Pivot = pivot;
        MarkDirty();
    }

    void SetMargin(const Margin& margin) {
        m_Margin = margin;
        MarkDirty();
    }

    void SetVisibility(Visibility vis) {
        m_Visibility = vis;
        MarkDirty();
    }

    void SetInteractable(bool interactable) {
        m_Interactable = interactable;
    }

    void SetName(const std::string& name) { m_Name = name; }

    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] U32 GetId() const { return m_Id; }
    [[nodiscard]] const Rect& GetWorldRect() const { return m_WorldRect; }
    [[nodiscard]] const Rect& GetLocalRect() const { return m_LocalRect; }
    [[nodiscard]] const Vector<UIElementPtr>& GetChildren() const { return m_Children; }
    [[nodiscard]] UIElementPtr GetParent() const { return m_Parent.lock(); }
    [[nodiscard]] bool IsVisible() const { return m_Visibility == Visibility::Visible; }
    [[nodiscard]] bool IsInteractable() const { return m_Interactable; }
    [[nodiscard]] const Math::Vec2& GetSizeDelta() const { return m_SizeDelta; }

protected:
    virtual void OnDraw(UIRenderer*) {}
    virtual void OnMouseDown(const Math::Vec2&) {}
    virtual void OnMouseUp(const Math::Vec2&) {}
    virtual void OnMouseHover(const Math::Vec2&) {}

    void MarkDirty() {
        m_DirtyLayout = true;
        m_DirtyTransform = true;
        for (auto& child : m_Children) {
            child->MarkDirty();
        }
    }

    void CalculateLocalRect() {
        if (auto parent{m_Parent.lock()}) {
            const Rect& pr{parent->GetLocalRect()};
            Math::Vec2 aMin{m_Anchor.min.x * pr.size.x, m_Anchor.min.y * pr.size.y};
            Math::Vec2 aMax{m_Anchor.max.x * pr.size.x, m_Anchor.max.y * pr.size.y};

            const bool stretchX{m_Anchor.min.x != m_Anchor.max.x};
            const bool stretchY{m_Anchor.min.y != m_Anchor.max.y};

            Math::Vec2 size{};
            size.x = stretchX ? (aMax.x - aMin.x - m_Margin.left - m_Margin.right) : m_SizeDelta.x;
            size.y = stretchY ? (aMax.y - aMin.y - m_Margin.top - m_Margin.bottom) : m_SizeDelta.y;
            assert(size.x >= 0.0f && size.y >= 0.0f, "negative size");

            Math::Vec2 pos{};
            pos.x = stretchX ? (aMin.x + m_Margin.left + m_AnchoredPosition.x)
                             : (aMin.x + m_AnchoredPosition.x - m_Pivot.x * size.x);
            pos.y = stretchY ? (aMin.y + m_Margin.top + m_AnchoredPosition.y)
                             : (aMin.y + m_AnchoredPosition.y - m_Pivot.y * size.y);

            m_LocalRect = Rect{pos, size};
        } else {
            Math::Vec2 pivotOffset{m_SizeDelta.x * m_Pivot.x, m_SizeDelta.y * m_Pivot.y};
            m_LocalRect = Rect{m_AnchoredPosition - pivotOffset, m_SizeDelta};
        }
    }

    void CalculateWorldRect() {
        if (auto parent = m_Parent.lock()) {
            const Rect& parentWorld = parent->GetWorldRect();
            m_WorldRect = Rect{
                parentWorld.position + m_LocalRect.position,
                m_LocalRect.size
            };
        } else {
            m_WorldRect = m_LocalRect;
        }
    }
};