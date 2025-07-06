// Pixel Shader for PulsingGutsSelfExercise
// Target: ps_5_0

cbuffer Constants : register(b0) {
    float2 iResolution;
    float iTime;
    float pad;
};

struct PSInput {
    float4 position : SV_POSITION;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float2 fragCoord = input.position.xy;
    fragCoord.y = iResolution.y - fragCoord.y;
    float2 normalizedPos = (fragCoord - 0.5f * iResolution) / iResolution.y;
    float time = iTime;
    
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    float2 warpedPosition = float2(0.0f, 0.0f);
    float distFromCenter = dot(normalizedPos, normalizedPos);
    float scale = 12.0f;
    float accum = 0.0f;
    
    const float SPEED = 4.0f;
    const float CRANK = time * SPEED;
    const float CYCLE = sin(CRANK - distFromCenter * 6.0f);
    
    warpedPosition =  CRANK + CYCLE;
    accum += dot(cos(warpedPosition) / scale, float2(0.2f, 0.2f));
    
    const float3 ORANGE = float3(4.0f, 2.0f, 1.0f);
    const float CONTRAST = 2.0f;
    const float DIM_FACTOR = 0.2f;
    
    finalColor = ORANGE * (accum + DIM_FACTOR) + (accum * CONTRAST) - distFromCenter;
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
}