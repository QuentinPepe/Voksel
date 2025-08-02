cbuffer CameraConstants : register(b0) {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3 cameraPosition;
    float padding;
};

cbuffer ObjectConstants : register(b1) {
    float4x4 world;
};

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;

    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.worldPos = worldPos.xyz;

    output.position = mul(worldPos, viewProjection);
    output.color = input.color;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}