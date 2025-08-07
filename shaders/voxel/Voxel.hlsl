cbuffer CameraCB : register(b0) {
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float3   gCameraPos; float _pad0;
}

cbuffer ObjectCB : register(b1) {
    float4x4 gWorld;
}

struct VSIn {
    float3 pos   : POSITION;
    uint   color : COLOR;
};

struct VSOut {
    float4 svpos : SV_Position;
    float4 color : COLOR0;
};

float4 UnpackRGBA8(uint c) {
    float r = (c & 0xFF) / 255.0f;
    float g = ((c >> 8) & 0xFF) / 255.0f;
    float b = ((c >> 16) & 0xFF) / 255.0f;
    float a = ((c >> 24) & 0xFF) / 255.0f;
    return float4(r, g, b, a);
}

VSOut VSMain(VSIn input) {
    VSOut o;
    float4 wp = mul(float4(input.pos, 1.0f), gWorld);
    o.svpos = mul(wp, gViewProj);
    o.color = UnpackRGBA8(input.color);
    return o;
}

float4 PSMain(VSOut input) : SV_Target {
    return input.color;
}
