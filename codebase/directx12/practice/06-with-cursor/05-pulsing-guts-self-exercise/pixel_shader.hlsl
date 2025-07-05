// Pixel Shader for PulsingGutsSelfExercise
// Target: ps_5_0

cbuffer Constants : register(b0) {
    float2 iResolution;
    float  iTime;
    float  pad;
};

struct PSInput {
    float4 position : SV_POSITION;
};

float2x2 rotate2D(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float2x2(c, s, -s, c);
}

float4 PSMain(PSInput input) : SV_TARGET {
    float2 fragCoord = input.position.xy;
    // Flip Y to match ShaderToy's bottom-left origin
    fragCoord.y = iResolution.y - fragCoord.y;
    // Normalized, centered pixel coordinates (aspect-correct)
    float2 normalizedPos = (fragCoord - 0.5f * iResolution) / iResolution.y;
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    float time = iTime;
    float2 noiseOffset = float2(0.0f, 0.0f);
    float2 warpedPos = float2(0.0f, 0.0f);
    float2 iterPos = normalizedPos;
    float distFromCenter = dot(iterPos, iterPos);
    float scale = 12.0f;
    float accum = 0.0f;
    float2x2 rotationMatrix = rotate2D(5.0f);
    [loop]
    for (float iter = 0.0f; iter < 20.0f; iter += 1.0f) {
        iterPos = mul(iterPos, rotationMatrix);
        noiseOffset = mul(noiseOffset, rotationMatrix);
        warpedPos = iterPos * scale + time * 4.0f + sin(time * 4.0f - distFromCenter * 6.0f) * 0.8f + iter + noiseOffset;
        accum += dot(cos(warpedPos) / scale, float2(0.2f, 0.2f));
        noiseOffset -= sin(warpedPos);
        scale *= 1.2f;
    }
    finalColor = float3(4.0f, 2.0f, 1.0f) * (accum + 0.2f) + accum + accum - distFromCenter;
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
} 