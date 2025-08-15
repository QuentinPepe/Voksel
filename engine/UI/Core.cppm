export module UI.Core;

import Core.Types;
import Math.Vector;
import Math.Matrix;
import std;

export enum class AnchorPreset : U8 {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    StretchLeft,
    StretchRight,
    StretchTop,
    StretchBottom,
    StretchAll,
    Custom
};

export struct Anchor {
    Math::Vec2 min{0.0f, 0.0f};
    Math::Vec2 max{0.0f, 0.0f};

    constexpr Anchor() = default;
    constexpr Anchor(F32 x, F32 y) : min{x, y}, max{x, y} {}
    constexpr Anchor(F32 minX, F32 minY, F32 maxX, F32 maxY) : min{minX, minY}, max{maxX, maxY} {}
    constexpr Anchor(const Math::Vec2& point) : min{point}, max{point} {}
    constexpr Anchor(const Math::Vec2& min_, const Math::Vec2& max_) : min{min_}, max{max_} {}

    static constexpr Anchor FromPreset(AnchorPreset preset) {
        switch (preset) {
            case AnchorPreset::TopLeft:      return {0.0f, 0.0f};
            case AnchorPreset::TopCenter:    return {0.5f, 0.0f};
            case AnchorPreset::TopRight:     return {1.0f, 0.0f};
            case AnchorPreset::MiddleLeft:   return {0.0f, 0.5f};
            case AnchorPreset::Center:       return {0.5f, 0.5f};
            case AnchorPreset::MiddleRight:  return {1.0f, 0.5f};
            case AnchorPreset::BottomLeft:   return {0.0f, 1.0f};
            case AnchorPreset::BottomCenter: return {0.5f, 1.0f};
            case AnchorPreset::BottomRight:  return {1.0f, 1.0f};
            case AnchorPreset::StretchLeft:  return {0.0f, 0.0f, 0.0f, 1.0f};
            case AnchorPreset::StretchRight: return {1.0f, 0.0f, 1.0f, 1.0f};
            case AnchorPreset::StretchTop:   return {0.0f, 0.0f, 1.0f, 0.0f};
            case AnchorPreset::StretchBottom:return {0.0f, 1.0f, 1.0f, 1.0f};
            case AnchorPreset::StretchAll:   return {0.0f, 0.0f, 1.0f, 1.0f};
            default:                         return {0.5f, 0.5f};
        }
    }
};

export struct Margin {
    F32 left{0.0f};
    F32 top{0.0f};
    F32 right{0.0f};
    F32 bottom{0.0f};

    constexpr Margin() = default;
    constexpr Margin(F32 all) : left{all}, top{all}, right{all}, bottom{all} {}
    constexpr Margin(F32 horizontal, F32 vertical) : left{horizontal}, top{vertical}, right{horizontal}, bottom{vertical} {}
    constexpr Margin(F32 l, F32 t, F32 r, F32 b) : left{l}, top{t}, right{r}, bottom{b} {}
};

export struct Rect {
    Math::Vec2 position{};
    Math::Vec2 size{};

    constexpr Rect() = default;
    constexpr Rect(F32 x, F32 y, F32 w, F32 h) : position{x, y}, size{w, h} {}
    constexpr Rect(const Math::Vec2& pos, const Math::Vec2& sz) : position{pos}, size{sz} {}

    [[nodiscard]] constexpr F32 Left() const { return position.x; }
    [[nodiscard]] constexpr F32 Top() const { return position.y; }
    [[nodiscard]] constexpr F32 Right() const { return position.x + size.x; }
    [[nodiscard]] constexpr F32 Bottom() const { return position.y + size.y; }
    [[nodiscard]] constexpr Math::Vec2 Center() const { return position + size * 0.5f; }

    [[nodiscard]] constexpr bool Contains(const Math::Vec2& point) const {
        return point.x >= position.x && point.x <= position.x + size.x &&
               point.y >= position.y && point.y <= position.y + size.y;
    }

    [[nodiscard]] constexpr bool Intersects(const Rect& other) const {
        return !(Right() < other.Left() || Left() > other.Right() ||
                Bottom() < other.Top() || Top() > other.Bottom());
    }
};

export enum class Visibility : U8 {
    Visible,
    Hidden,
    Collapsed
};

export enum class LayoutDirection : U8 {
    Horizontal,
    Vertical
};

export enum class Alignment : U8 {
    Start,
    Center,
    End,
    Stretch
};

export struct Color {
    F32 r{1.0f}, g{1.0f}, b{1.0f}, a{1.0f};

    constexpr Color() = default;
    constexpr Color(F32 r_, F32 g_, F32 b_, F32 a_ = 1.0f) : r{r_}, g{g_}, b{b_}, a{a_} {}
    constexpr Color(U32 rgba) :
        r{((rgba >> 24) & 0xFF) / 255.0f},
        g{((rgba >> 16) & 0xFF) / 255.0f},
        b{((rgba >> 8) & 0xFF) / 255.0f},
        a{(rgba & 0xFF) / 255.0f} {}

    [[nodiscard]] constexpr U32 ToRGBA() const {
        return (static_cast<U32>(r * 255.0f) << 24) |
               (static_cast<U32>(g * 255.0f) << 16) |
               (static_cast<U32>(b * 255.0f) << 8) |
               static_cast<U32>(a * 255.0f);
    }

    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Transparent;
};

inline const Color Color::White{1.0f, 1.0f, 1.0f, 1.0f};
inline const Color Color::Black{0.0f, 0.0f, 0.0f, 1.0f};
inline const Color Color::Red{1.0f, 0.0f, 0.0f, 1.0f};
inline const Color Color::Green{0.0f, 1.0f, 0.0f, 1.0f};
inline const Color Color::Blue{0.0f, 0.0f, 1.0f, 1.0f};
inline const Color Color::Transparent{0.0f, 0.0f, 0.0f, 0.0f};