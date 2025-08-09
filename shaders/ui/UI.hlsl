cbuffer UICommonCB : register(b0) {
    float2 gViewSize; float2 _pad0;
}
cbuffer UIInstanceCB : register(b2) {
    // Max 1024 items * 48B = 49KB < 64KB CB limit
    struct UIInst {
        float2 pos;    // pixels TL
        float2 size;   // pixels
        float4 uv;     // xy=min zw=max
        uint   color;  // RGBA8
        float  radius; // px
        uint   flags;  // bit0: textured
        uint   _padA;  // align
    };
    UIInst gInst[1024];
}

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

float4 UnpackRGBA8(uint c) {
    float4 v = float4(
        (c      ) & 255,
        (c >>  8) & 255,
        (c >> 16) & 255,
        (c >> 24) & 255
    ) / 255.0;
    return v;
}

struct VSOut {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float2 local : TEXCOORD1; // from center
    float2 size  : TEXCOORD2;
    float4 color : COLOR0;
    float  rad   : TEXCOORD3;
    uint   flags : TEXCOORD4;
};

VSOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    VSOut o;
    UIInst inst = gInst[iid];

    // unit quad (0,0)-(1,1) expanded into 2 tris (vid 0..5)
    float2 q[6] = {
        float2(0,0), float2(1,0), float2(1,1),
        float2(0,0), float2(1,1), float2(0,1)
    };
    float2 t = q[vid];

    float2 p = inst.pos + t * inst.size;
    float2 ndc;
    ndc.x = (p.x / gViewSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (p.y / gViewSize.y) * 2.0;

    o.pos = float4(ndc, 0.0, 1.0);
    o.size = inst.size;
    o.local = (p - (inst.pos + 0.5 * inst.size)); // center space
    o.rad = inst.radius;
    o.flags = inst.flags;
    o.color = UnpackRGBA8(inst.color);

    float2 uv01 = t;
    float2 uv = lerp(inst.uv.xy, inst.uv.zw, uv01);
    o.uv = uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target {
    float4 col = (i.flags & 1u) ? gTex.Sample(gSamp, i.uv) * i.color : i.color;

    if (i.rad > 0.0) {
        float2 h = 0.5 * i.size;
        float2 d = abs(i.local) - (h - i.rad);
        float outside = length(max(d, 0.0)) - i.rad;
        float aa = 1.0 - saturate(outside + 0.5); // 1px AA
        col.a *= aa;
        if (col.a <= 0.001) discard;
    }

    return col;
}
