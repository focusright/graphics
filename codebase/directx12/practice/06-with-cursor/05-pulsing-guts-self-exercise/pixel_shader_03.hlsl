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
    
    float distFromCenter = dot(normalizedPos, normalizedPos);
    
    const float SPEED = 4.0f;
    const float PULSE_INTENSITY = 0.8f;
    const float CRANK = time * SPEED;
    const float FREQUENCY = 6.0f;
    const float PHASE_OFFSET = distFromCenter * FREQUENCY;
    const float CYCLE = sin(CRANK - PHASE_OFFSET) * PULSE_INTENSITY;
    
    /* In the wave equation y(t) = A * sin(Bt - C) + D: 
       A represents the amplitude of the wave
       B determines the frequency and period of the wave
       C represents the horizontal shift
       D represents the vertical shift  */
    
    float scale = 12.0f; //Change to negative value to spiral towards upper right corner
    float warpScale = scale;  //scale separated into two variables in order to change
    float colorScale = scale; //separately in experimentation. Change warpScale only to stretch the spiral effect
    
    float2 spiralOut = normalizedPos * warpScale; //Expands the coordinate space, makes the animation stretch outward from center
    float2 warpedPosition = spiralOut + CRANK + CYCLE; //Creates the spiral/warping visual effect
    float accum = dot(cos(warpedPosition) / colorScale, float2(0.2f, 0.2f)); //Normalizes the intensity to keep values in reasonable range
    //Divides by the same scale to "undo" the expansion
    //Prevents the color from becoming too bright or washed out
    
    const float3 ORANGE = float3(4.0f, 2.0f, 1.0f);
    const float CONTRAST = 2.0f;
    const float DIM_FACTOR = 0.2f;
    
    float3 finalColor = ORANGE * (accum + DIM_FACTOR) + (accum * CONTRAST) - distFromCenter;
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
} 