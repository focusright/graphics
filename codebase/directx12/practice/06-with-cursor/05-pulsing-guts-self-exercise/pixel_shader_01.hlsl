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

float4 PSMain(PSInput input) : SV_TARGET {
    float2 fragCoord = input.position.xy;
    // Flip Y to match ShaderToy's bottom-left origin
    fragCoord.y = iResolution.y - fragCoord.y;
    const float2 normalizedPos = (fragCoord - 0.5f * iResolution) / iResolution.y;
    //distFromCenter = squared distance from the center (0,0) to the current pixel
    const float distFromCenter = dot(normalizedPos, normalizedPos);
    
    //The color range is clamped to [0.0, 1.0]
    //Without the dimming factor, one of the following will work:
    
    //const float3 ORANGE = float3(1.0f, 0.5f, 0.25f);
    //const float3 ORANGE = float3(4.0f, 2.0f, 1.0f) / 4.0f; // = (1.0f, 0.5f, 0.25f)
    //const float3 ORANGE = normalize(float3(4.0f, 2.0f, 1.0f));
    
    const float3 ORANGE = float3(4.0f, 2.0f, 1.0f);
    
    const float DIM_FACTOR = 0.2f;
    const float3 finalColor = (ORANGE * DIM_FACTOR) - distFromCenter;
    
    //Without the dimming factor:
    //const float3 finalColor = ORANGE - distFromCenter;
    
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
} 