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
    fragCoord.y = iResolution.y - fragCoord.y;
    float2 normalizedPos = (fragCoord - 0.5f * iResolution) / iResolution.y;
    float time = iTime;
    
    float2 noiseOffset = float2(0.0f, 0.0f);
    float2 warpedPosition = float2(0.0f, 0.0f);
    float2 spiralOut = float2(0.0f, 0.0f);
    float2 iterationPosition = normalizedPos;
    float distFromCenter = dot(iterationPosition, iterationPosition);
    float scale = 12.0f;
    float accum = 0.0f;
    
    const float SPEED = 4.0f;
    const float PULSE_INTENSITY = 0.8f;
    const float CRANK = time * SPEED;
    const float FREQUENCY = 6.0f;
    const float PHASE_OFFSET = distFromCenter * FREQUENCY;
    const float CYCLE = sin(CRANK - PHASE_OFFSET) * PULSE_INTENSITY;
    
    float iterations = 20.0f; //Change this from low to high to see lines come into focus
    [loop]
    for (float i = 0.0f; i < iterations; i += 1.0f) {
        spiralOut = normalizedPos * scale;
        warpedPosition = spiralOut + CRANK + CYCLE + i + noiseOffset; //i gives it large values
        accum += dot(cos(warpedPosition) / scale, float2(0.2f, 0.2f));
        //Sine at large values creates rapid oscillations,
        //wraps around the unit circle quickly and looks random
        noiseOffset -= sin(warpedPosition);
    }
    
    const float3 ORANGE = float3(4.0f, 2.0f, 1.0f);
    const float CONTRAST = 2.0f;
    const float DIM_FACTOR = 0.2f;
    
    float3 finalColor = ORANGE * (accum + DIM_FACTOR) + (accum * CONTRAST) - distFromCenter;
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
} 