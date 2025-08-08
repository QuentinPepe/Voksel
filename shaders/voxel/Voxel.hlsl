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
    float3 pos   : POSITION;
    float3 nrm   : NORMAL;
    float2 uv    : TEXCOORD;
    uint   mat   : COLOR;
};
struct VSOut {
    float4 svpos : SV_Position;
    float3 nrm   : NORMAL0;
    float2 uv    : TEXCOORD0;
    uint   mat   : COLOR0;
};
VSOut VSMain(VSIn input) {
    VSOut o;
    float4 wp = mul(float4(input.pos, 1.0f), gWorld);
    o.svpos = mul(wp, gViewProj);
    o.nrm = normalize(mul(float4(input.nrm,0.0f), gWorld).xyz);
    o.uv = input.uv;
    o.mat = input.mat;
    return o;
}
float4 PSMain(VSOut input) : SV_Target {
    uint tileIndex = input.mat;
    uint tx = tileIndex % tilesX;
    uint ty = tileIndex / tilesX;
    float2 basePx = float2(tx * (tileSize + 2 * pad) + pad, ty * (tileSize + 2 * pad) + pad);
    float2 spanPx = float2(tileSize - 2 * pad, tileSize - 2 * pad);
    float2 fuv = frac(input.uv);
    float2 atlasPx = basePx + fuv * spanPx;
    float2 atlasUV = atlasPx / float2(atlasW, atlasH);
    float4 albedo = gAtlas.Sample(gSamp, atlasUV);
    return albedo;
}
