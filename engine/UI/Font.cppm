module;
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

export module UI.Font;

import Core.Types;
import Core.Assert;
import Math.Vector;
import Graphics;
import std;

export class UIFont {
private:
    Vector<U8> m_TTF{};
    stbtt_fontinfo m_Info{};
    F32 m_Ascent{0.0f}, m_Descent{0.0f}, m_LineGap{0.0f};
    F32 m_BakePx{48.0f};
    F32 m_BakeScale{1.0f};

    U32 m_AtlasW{1024};
    U32 m_AtlasH{1024};
    Vector<U8> m_AtlasRGBA{};
    Vector<stbtt_packedchar> m_Packed{}; // [32..126]
    U32 m_Texture{INVALID_INDEX};

    static constexpr U32 First{32};
    static constexpr U32 Last{126};

public:
    UIFont() = default;

    bool LoadFromFile(IGraphicsContext* gfx, const std::string& path, F32 bakePx = 48.0f, U32 atlasW = 1024, U32 atlasH = 1024) {
        assert(gfx, "null gfx");
        std::ifstream f{path, std::ios::binary | std::ios::ate};
        if (!f) return false;
        auto sz{static_cast<size_t>(f.tellg())};
        f.seekg(0, std::ios::beg);
        m_TTF.resize(sz);
        f.read(reinterpret_cast<char*>(m_TTF.data()), sz);
        return LoadFromMemory(gfx, m_TTF.data(), static_cast<int>(m_TTF.size()), bakePx, atlasW, atlasH);
    }

    bool LoadFromMemory(IGraphicsContext* gfx, const U8* data, int size, F32 bakePx = 48.0f, U32 atlasW = 1024, U32 atlasH = 1024) {
        assert(gfx, "null gfx");
        assert(data && size > 0, "invalid font data");
        assert(stbtt_InitFont(&m_Info, data, stbtt_GetFontOffsetForIndex(data, 0)), "font init failed");

        m_BakePx = bakePx;
        m_BakeScale = stbtt_ScaleForPixelHeight(&m_Info, m_BakePx);

        int a{}, d{}, g{};
        stbtt_GetFontVMetrics(&m_Info, &a, &d, &g);
        m_Ascent = static_cast<F32>(a) * m_BakeScale;
        m_Descent = static_cast<F32>(d) * m_BakeScale;
        m_LineGap = static_cast<F32>(g) * m_BakeScale;

        m_AtlasW = atlasW;
        m_AtlasH = atlasH;
        Vector<U8> alpha{};
        alpha.resize(static_cast<size_t>(m_AtlasW) * m_AtlasH);
        std::fill(alpha.begin(), alpha.end(), 0);

        stbtt_pack_context pc{};
        assert(stbtt_PackBegin(&pc, alpha.data(), static_cast<int>(m_AtlasW), static_cast<int>(m_AtlasH), 0, 1, nullptr) != 0, "pack begin failed");
        stbtt_PackSetOversampling(&pc, 3, 3);

        m_Packed.resize(Last - First + 1);
        assert(stbtt_PackFontRange(&pc, m_Info.data, 0, m_BakePx, First, Last - First + 1, m_Packed.data()) != 0, "pack range failed");
        stbtt_PackEnd(&pc);

        m_AtlasRGBA.resize(static_cast<size_t>(m_AtlasW) * m_AtlasH * 4);
        for (U32 y{0}; y < m_AtlasH; ++y) {
            for (U32 x{0}; x < m_AtlasW; ++x) {
                size_t i{static_cast<size_t>(y) * m_AtlasW + x};
                U8 a8{alpha[i]};
                size_t o{i * 4};
                m_AtlasRGBA[o + 0] = 255;
                m_AtlasRGBA[o + 1] = 255;
                m_AtlasRGBA[o + 2] = 255;
                m_AtlasRGBA[o + 3] = a8;
            }
        }

        m_Texture = gfx->CreateTexture2D(m_AtlasRGBA.data(), m_AtlasW, m_AtlasH, 1);
        return m_Texture != INVALID_INDEX;
    }

    [[nodiscard]] U32 GetTexture() const { return m_Texture; }
    [[nodiscard]] F32 GetAscent() const { return m_Ascent; }
    [[nodiscard]] F32 GetDescent() const { return m_Descent; }
    [[nodiscard]] F32 GetLineGap() const { return m_LineGap; }
    [[nodiscard]] F32 GetBakePx() const { return m_BakePx; }

    F32 KerningAtBakePx(U32 cp0, U32 cp1) const {
        int k{stbtt_GetCodepointKernAdvance(&m_Info, static_cast<int>(cp0), static_cast<int>(cp1))};
        return static_cast<F32>(k) * m_BakeScale;
    }

    bool GetQuad(U32 codepoint, F32 x, F32 y, F32& x0, F32& y0, F32& x1, F32& y1, F32& u0, F32& v0, F32& u1, F32& v1, F32& xAdvance) const {
        U32 cp{codepoint};
        if (cp < First || cp > Last) cp = static_cast<U32>('?');
        stbtt_aligned_quad q{};
        stbtt_GetPackedQuad(m_Packed.data(), static_cast<int>(m_AtlasW), static_cast<int>(m_AtlasH),
                            static_cast<int>(cp - First), &x, &y, &q, 0);
        x0 = q.x0; y0 = q.y0; x1 = q.x1; y1 = q.y1;
        u0 = q.s0; v0 = q.t0; u1 = q.s1; v1 = q.t1;

        // Approximate advance at bake px
        float advBake{};
        int g = stbtt_FindGlyphIndex(&m_Info, static_cast<int>(cp));
        int adv{}, lsb{};
        stbtt_GetGlyphHMetrics(&m_Info, g, &adv, &lsb);
        advBake = static_cast<float>(adv) * m_BakeScale;
        xAdvance = advBake;
        return true;
    }

    Math::Vec2 MeasureText(const std::string& utf8, F32 px) const {
        F32 s{px / m_BakePx};
        F32 x{0.0f}, y{0.0f};
        F32 maxX{0.0f};
        U32 prev{0};
        for (size_t i{0}; i < utf8.size();) {
            U32 cp{};
            size_t n{DecodeUTF8(utf8, i, cp)};
            if (n == 0) break;
            i += n;
            if (cp == '\n') {
                maxX = std::max(maxX, x);
                x = 0.0f;
                y += (m_Ascent - m_Descent + m_LineGap);
                prev = 0;
                continue;
            }
            F32 x0{}, y0{}, x1{}, y1{}, u0{}, v0{}, u1{}, v1{}, adv{};
            GetQuad(cp, x, 0.0f, x0, y0, x1, y1, u0, v0, u1, v1, adv);
            if (prev) x += KerningAtBakePx(prev, cp);
            x += adv;
            prev = cp;
        }
        maxX = std::max(maxX, x);
        F32 h{m_Ascent - m_Descent};
        return {maxX * s, (y + h) * s};
    }

    static size_t DecodeUTF8(const std::string& s, size_t i, U32& cp) {
        U8 c{static_cast<U8>(s[i])};
        if ((c & 0x80u) == 0u) { cp = c; return 1; }
        if ((c & 0xE0u) == 0xC0u && i + 1 < s.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<U8>(s[i + 1]) & 0x3Fu); return 2;
        }
        if ((c & 0xF0u) == 0xE0u && i + 2 < s.size()) {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<U8>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<U8>(s[i + 2]) & 0x3Fu); return 3;
        }
        if ((c & 0xF8u) == 0xF0u && i + 3 < s.size()) {
            cp = ((c & 0x07u) << 18) | ((static_cast<U8>(s[i + 1]) & 0x3Fu) << 12) | ((static_cast<U8>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<U8>(s[i + 3]) & 0x3Fu); return 4;
        }
        cp = '?'; return 1;
    }
};
