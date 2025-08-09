export module UI.Core;

import Core.Types;
import Core.Assert;
import Math.Vector;
import Math.Core;
import std;

export enum class UIDirection : U8 { Row, Column };

export struct UIUnit {
    enum class Kind : U8 { Px, Percent, Auto };
    Kind kind{Kind::Auto};
    F32 value{0.0f};
    static UIUnit Px(F32 v){ return UIUnit{Kind::Px, v}; }
    static UIUnit Percent(F32 v){ return UIUnit{Kind::Percent, v}; }
    static UIUnit Auto(){ return UIUnit{Kind::Auto, 0.0f}; }
};

export struct UILayout {
    UIDirection dir{UIDirection::Column};
    UIUnit width{UIUnit::Auto()};
    UIUnit height{UIUnit::Auto()};
    F32 pad{0.0f};
    F32 gap{0.0f};
    Math::Vec2 anchor{0.0f, 0.0f};
    Math::Vec2 offset{0.0f, 0.0f};
};

export struct UIRect { F32 x, y, w, h; };

export struct UIInput {
    F32 mx{0}, my{0};
    bool down{false};
    bool pressed{false};
    bool released{false};
};

export class UIContext {
public:
    struct DrawSink {
        virtual Math::Vec2 ViewSize() const = 0;
        virtual void PushQuad(U32 tex, UIRect r, Math::Vec4 uv, U32 color, F32 radius) = 0;
        virtual const UIInput& Input() const = 0;
        virtual F32 Time01() const = 0;
    };

    explicit UIContext(DrawSink* s) : m_Sink{s} {
        assert(m_Sink != nullptr, "null sink");
        Math::Vec2 v{m_Sink->ViewSize()};
        m_Stack.reserve(32);
        m_Stack.push_back(Node{UIRect{0,0,v.x,v.y}, UIDirection::Column, 0.0f, 0.0f, 0.0f});
    }

    struct Scope { USize idx; };

    Scope Begin(UILayout lay, U32 bg = 0, F32 radius = 0.0f) {
        UIRect parent{m_Stack.back().rect};
        UIRect r{Resolve(parent, lay)};
        if (bg) m_Sink->PushQuad(0, r, Math::Vec4{0,0,1,1}, bg, radius);
        Node n{};
        n.rect = Inner(r, lay.pad);
        n.dir = lay.dir;
        n.cursorX = n.rect.x;
        n.cursorY = n.rect.y;
        n.gap = lay.gap;
        m_Stack.push_back(n);
        return Scope{static_cast<USize>(m_Stack.size()-1)};
    }

    void End(Scope) {
        assert(m_Stack.size()>1, "stack underflow");
        m_Stack.pop_back();
    }

    bool Button(std::string_view id, UILayout lay, U32 color, F32 radius) {
        (void)id;
        UIRect r{Place(lay)};
        bool hot{Hit(r)};
        U32 c{ hot ? ColorMul(color, 1.1f) : color };
        m_Sink->PushQuad(0, r, Math::Vec4{0,0,1,1}, c, radius);
        return hot && m_Sink->Input().pressed;
    }

    void Progress(UILayout lay, F32 t01, U32 back, U32 fill, F32 radius) {
        UIRect r{Place(lay)};
        m_Sink->PushQuad(0, r, Math::Vec4{0,0,1,1}, back, radius);
        t01 = Math::Clamp(t01, 0.0f, 1.0f);
        UIRect rf{r.x, r.y, r.w * t01, r.h};
        m_Sink->PushQuad(0, rf, Math::Vec4{0,0,1,1}, fill, radius);
    }

    void Image(U32 texture, UILayout lay, Math::Vec4 uv, F32 radius = 0.0f, U32 tint = 0xFFFFFFFFu) {
        UIRect r{Place(lay)};
        m_Sink->PushQuad(texture ? texture : 0, r, uv, tint, radius);
    }

    F32 Time01() const { return m_Sink->Time01(); }

private:
    struct Node {
        UIRect rect;
        UIDirection dir;
        F32 cursorX;
        F32 cursorY;
        F32 gap;
    };

    std::vector<Node> m_Stack;
    DrawSink* m_Sink{};

    static U32 ColorMul(U32 c, F32 k) {
        U32 r = std::min<U32>(255u, static_cast<U32>(((c      ) & 255u) * k));
        U32 g = std::min<U32>(255u, static_cast<U32>(((c >>  8) & 255u) * k));
        U32 b = std::min<U32>(255u, static_cast<U32>(((c >> 16) & 255u) * k));
        U32 a = (c >> 24) & 255u;
        return r | (g<<8) | (b<<16) | (a<<24);
    }

    static UIRect Inner(UIRect r, F32 pad){
        return UIRect{r.x+pad, r.y+pad, std::max(0.0f, r.w-2*pad), std::max(0.0f, r.h-2*pad)};
    }

    static F32 ResolveUnit(UIUnit u, F32 avail){
        if (u.kind == UIUnit::Kind::Px) return u.value;
        if (u.kind == UIUnit::Kind::Percent) return avail * u.value;
        return avail;
    }

    UIRect Resolve(UIRect parent, const UILayout& lay){
        Math::Vec2 vs{parent.w, parent.h};
        F32 w{ResolveUnit(lay.width, vs.x)};
        F32 h{ResolveUnit(lay.height, vs.y)};
        F32 px{parent.x + lay.anchor.x * (vs.x - w) + lay.offset.x};
        F32 py{parent.y + lay.anchor.y * (vs.y - h) + lay.offset.y};
        return UIRect{px, py, w, h};
    }

    UIRect Place(const UILayout& c){
        Node& n{m_Stack.back()};
        UIRect r{};
        if (n.dir == UIDirection::Row) {
            r = ResolveRow(n, c);
            n.cursorX = r.x + r.w + n.gap;
        } else {
            r = ResolveCol(n, c);
            n.cursorY = r.y + r.h + n.gap;
        }
        return r;
    }

    UIRect ResolveRow(const Node& n, const UILayout& c){
        F32 h = (c.height.kind==UIUnit::Kind::Px) ? c.height.value :
                (c.height.kind==UIUnit::Kind::Percent ? n.rect.h * c.height.value : n.rect.h);
        F32 w = (c.width.kind==UIUnit::Kind::Px) ? c.width.value :
                (c.width.kind==UIUnit::Kind::Percent ? n.rect.w * c.width.value : n.rect.w);
        return UIRect{n.cursorX, n.cursorY, w, h};
    }

    UIRect ResolveCol(const Node& n, const UILayout& c){
        F32 w = (c.width.kind==UIUnit::Kind::Px) ? c.width.value :
                (c.width.kind==UIUnit::Kind::Percent ? n.rect.w * c.width.value : n.rect.w);
        F32 h = (c.height.kind==UIUnit::Kind::Px) ? c.height.value :
                (c.height.kind==UIUnit::Kind::Percent ? n.rect.h * c.height.value : n.rect.h);
        return UIRect{n.cursorX, n.cursorY, w, h};
    }

    bool Hit(UIRect r) const {
        auto const& in{m_Sink->Input()};
        return in.mx >= r.x && in.mx <= r.x + r.w && in.my >= r.y && in.my <= r.y + r.h;
    }
};
