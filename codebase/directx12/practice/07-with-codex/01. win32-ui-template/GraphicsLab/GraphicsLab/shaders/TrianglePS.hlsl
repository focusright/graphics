struct PixelInput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

float4 PSMain(PixelInput input) : SV_Target {
    return float4(input.color, 1.0f);
}
