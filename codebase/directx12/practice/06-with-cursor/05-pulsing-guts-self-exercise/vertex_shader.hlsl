// Vertex Shader for PulsingGutsSelfExercise
// Target: vs_5_0

struct PSInput {
    float4 position : SV_POSITION;
};

PSInput VSMain(float3 position : POSITION) {
    PSInput result;
    result.position = float4(position.x, position.y, position.z, 1.0f);
    return result;
} 