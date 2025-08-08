cbuffer CameraCB : register(b0) {
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float3   gCameraPos; float _pad0;
}
cbuffer ObjectCB : register(b1) {
    float4x4 gWorld;
}
cbuffer AtlasCB : register(b2) {
    uint tilesX;
    uint tileSize;
    uint pad;
    uint _pad1;
    uint atlasW;
    uint atlasH;
}
Texture2D gAtlas : register(t0);
SamplerState gSamp : register(s0);

struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD;
    uint   mat : COLOR;
};
struct VSOut {
    float4 svpos : SV_Position;
    float3 nrm   : NORMAL0;
    float2 uv    : TEXCOORD0;
    uint   mat   : COLOR0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 wp = mul(float4(i.pos,1), gWorld);
    o.svpos = mul(wp, gViewProj);
    o.nrm = normalize(mul(float4(i.nrm,0), gWorld).xyz);

    float side = 1.0f - step(0.5f, abs(i.nrm.y));
    o.uv = float2(i.uv.x, lerp(i.uv.y, 1.0f - i.uv.y, side));
    o.mat = i.mat;
    return o;
}

float4 PSMain(VSOut i) : SV_Target {
    uint tx = i.mat % tilesX;
    uint ty = i.mat / tilesX;
    uint2 basePx = uint2(tx * (tileSize + 2 * pad) + pad,
                         ty * (tileSize + 2 * pad) + pad);

    float2 fuv = frac(i.uv);
    uint2 px = basePx + (uint2)floor(fuv * tileSize);

    return gAtlas.Load(int3(px, 0));
}
