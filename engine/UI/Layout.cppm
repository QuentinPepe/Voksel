export module UI.Layout;

import UI.Core;
import UI.Element;
import Core.Types;
import Math.Vector;
import std;

export class UILayout : public UIElement {
protected:
    LayoutDirection m_Direction{LayoutDirection::Vertical};
    Alignment m_ChildAlignment{Alignment::Start};
    F32 m_Spacing{0.0f};
    Margin m_Padding{};
    bool m_ChildControlWidth{true};
    bool m_ChildControlHeight{true};
    bool m_ChildForceExpandWidth{false};
    bool m_ChildForceExpandHeight{false};

public:
    void SetDirection(LayoutDirection dir) {
        m_Direction = dir;
        MarkDirty();
    }

    void SetChildAlignment(Alignment align) {
        m_ChildAlignment = align;
        MarkDirty();
    }

    void SetSpacing(F32 spacing) {
        m_Spacing = spacing;
        MarkDirty();
    }

    void SetPadding(const Margin& padding) {
        m_Padding = padding;
        MarkDirty();
    }

    void SetChildControl(bool width, bool height) {
        m_ChildControlWidth = width;
        m_ChildControlHeight = height;
        MarkDirty();
    }

    void SetChildForceExpand(bool width, bool height) {
        m_ChildForceExpandWidth = width;
        m_ChildForceExpandHeight = height;
        MarkDirty();
    }

    void UpdateLayout() override {
        UIElement::UpdateLayout();
        LayoutChildren();
    }

protected:
    virtual void LayoutChildren() {
        if (m_Children.empty()) return;

        const F32 contentWidth = m_LocalRect.size.x - m_Padding.left - m_Padding.right;
        const F32 contentHeight = m_LocalRect.size.y - m_Padding.top - m_Padding.bottom;
        const Math::Vec2 contentStart{m_Padding.left, m_Padding.top};

        USize visibleCount = 0;
        for (const auto& child : m_Children) {
            if (child->IsVisible()) visibleCount++;
        }

        if (visibleCount == 0) return;

        if (m_Direction == LayoutDirection::Vertical) {
            LayoutVertical(contentStart, contentWidth, contentHeight, visibleCount);
        } else {
            LayoutHorizontal(contentStart, contentWidth, contentHeight, visibleCount);
        }
    }

    void LayoutVertical(const Math::Vec2& start, F32 width, F32 height, USize count) {
        F32 totalSpacing = m_Spacing * static_cast<F32>(count - 1);
        F32 availableHeight = height - totalSpacing;
        F32 childHeight = m_ChildForceExpandHeight ? availableHeight / count : 0.0f;

        F32 currentY = start.y;

        for (const auto& child : m_Children) {
            if (!child->IsVisible()) continue;

            Math::Vec2 childSize = child->GetSizeDelta();

            if (m_ChildControlWidth) {
                childSize.x = width;
            }

            if (m_ChildControlHeight && m_ChildForceExpandHeight) {
                childSize.y = childHeight;
            }

            Math::Vec2 childPos{start.x, currentY};

            switch (m_ChildAlignment) {
                case Alignment::Center:
                    childPos.x += (width - childSize.x) * 0.5f;
                    break;
                case Alignment::End:
                    childPos.x += width - childSize.x;
                    break;
                case Alignment::Stretch:
                    childSize.x = width;
                    break;
                default:
                    break;
            }

            child->SetAnchor(AnchorPreset::TopLeft);
            child->SetPivot({0.0f, 0.0f});
            child->SetAnchoredPosition(childPos);
            child->SetSizeDelta(childSize);

            currentY += childSize.y + m_Spacing;
        }
    }

    void LayoutHorizontal(const Math::Vec2& start, F32 width, F32 height, USize count) {
        F32 totalSpacing = m_Spacing * static_cast<F32>(count - 1);
        F32 availableWidth = width - totalSpacing;
        F32 childWidth = m_ChildForceExpandWidth ? availableWidth / count : 0.0f;

        F32 currentX = start.x;

        for (const auto& child : m_Children) {
            if (!child->IsVisible()) continue;

            Math::Vec2 childSize = child->GetSizeDelta();

            if (m_ChildControlHeight) {
                childSize.y = height;
            }

            if (m_ChildControlWidth && m_ChildForceExpandWidth) {
                childSize.x = childWidth;
            }

            Math::Vec2 childPos{currentX, start.y};

            switch (m_ChildAlignment) {
                case Alignment::Center:
                    childPos.y += (height - childSize.y) * 0.5f;
                    break;
                case Alignment::End:
                    childPos.y += height - childSize.y;
                    break;
                case Alignment::Stretch:
                    childSize.y = height;
                    break;
                default:
                    break;
            }

            child->SetAnchor(AnchorPreset::TopLeft);
            child->SetPivot({0.0f, 0.0f});
            child->SetAnchoredPosition(childPos);
            child->SetSizeDelta(childSize);

            currentX += childSize.x + m_Spacing;
        }
    }
};

export class UIVerticalLayout : public UILayout {
public:
    UIVerticalLayout() {
        m_Direction = LayoutDirection::Vertical;
    }
};

export class UIHorizontalLayout : public UILayout {
public:
    UIHorizontalLayout() {
        m_Direction = LayoutDirection::Horizontal;
    }
};

export class UIGridLayout : public UIElement {
protected:
    U32 m_Columns{2};
    Math::Vec2 m_CellSize{100.0f, 100.0f};
    Math::Vec2 m_Spacing{5.0f, 5.0f};
    Margin m_Padding{};
    Alignment m_ChildAlignment{Alignment::Start};

public:
    void SetColumns(U32 columns) {
        m_Columns = std::max(1u, columns);
        MarkDirty();
    }

    void SetCellSize(const Math::Vec2& size) {
        m_CellSize = size;
        MarkDirty();
    }

    void SetSpacing(const Math::Vec2& spacing) {
        m_Spacing = spacing;
        MarkDirty();
    }

    void UpdateLayout() override {
        UIElement::UpdateLayout();
        LayoutGrid();
    }

protected:
    void LayoutGrid() {
        if (m_Children.empty() || m_Columns == 0) return;

        const Math::Vec2 startPos{m_Padding.left, m_Padding.top};

        U32 row = 0, col = 0;

        for (const auto& child : m_Children) {
            if (!child->IsVisible()) continue;

            Math::Vec2 cellPos = startPos + Math::Vec2{
                col * (m_CellSize.x + m_Spacing.x),
                row * (m_CellSize.y + m_Spacing.y)
            };

            child->SetAnchor(AnchorPreset::TopLeft);
            child->SetPivot({0.0f, 0.0f});
            child->SetAnchoredPosition(cellPos);
            child->SetSizeDelta(m_CellSize);

            col++;
            if (col >= m_Columns) {
                col = 0;
                row++;
            }
        }
    }
};