cbuffer Rotation : register(b0) {
    float angle;
};

struct VertexInput {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct PixelInput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

PixelInput VSMain(VertexInput input) {
    PixelInput output;
    float s;
    float c;
    sincos(angle, s, c);
    output.position = float4(
        input.position.x * c - input.position.y * s,
        input.position.x * s + input.position.y * c,
        input.position.z,
        1.0f);
    output.color = input.color;
    return output;
}
