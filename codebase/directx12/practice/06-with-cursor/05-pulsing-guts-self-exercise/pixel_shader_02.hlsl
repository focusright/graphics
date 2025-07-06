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
    
    float distFromCenter = dot(normalizedPos, normalizedPos);
    float scale = 12.0f;
    
    const float SPEED = 4.0f;
    const float CRANK = time * SPEED;
    const float FREQUENCY = 6.0f; //making the sine wave cycle 6 times faster
    const float PHASE_OFFSET = distFromCenter * FREQUENCY; //Increases with distance from center
    const float CYCLE = sin(CRANK - PHASE_OFFSET); //concentric wave pattern
    
    /* In a wave equation like y(t) = A * sin(Bt - C) + D, the C term represents 
       the phase shift. A positive C indicates a shift to the right, while a 
       negative C indicates a shift to the left. */ 
    
    //warpedPosition is essentially the core animation engine that drives the entire 
    //visual effect by combining temporal and spatial animation components.
    
    const float2 warpedPosition = CRANK + CYCLE;
    const float accum = dot(cos(warpedPosition) / scale, float2(0.2f, 0.2f));
    
    /* Since the first parameter is a scalar (single float) and the second is a 2D vector,
       this effectively:
         - Multiplies the scalar by each component of the vector
         - Sums the results: scalar * 0.2f + scalar * 0.2f = scalar * 0.4f */
 
    //accum is essentially the cosine of the warped position, scaled down by both the scale 
    //factor (12.0f) and the dot product (0.4f total).
    
    //accum ultimately controls how bright or dim each pixel appears based on the animated sine wave pattern.
    
    const float3 ORANGE = float3(4.0f, 2.0f, 1.0f);
    const float CONTRAST = 2.0f;
    const float DIM_FACTOR = 0.2f;
    
    float3 finalColor = ORANGE * (accum + DIM_FACTOR) + (accum * CONTRAST) - distFromCenter;
    return float4(finalColor.x, finalColor.y, finalColor.z, 1.0f);
}