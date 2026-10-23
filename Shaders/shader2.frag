#version 450

// Second subpass post-process.
// subpassLoad can only read THIS pixel (no neighbours), and there is no time uniform,
// so every effect here is built from per-pixel colour + depth + screen position.

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout(location = 0) out vec4 outColor;

// ---- settings --------------------------------------------------------------
// 0 = Sonar scanner   1 = Toon + fog   2 = Depth view   3 = Passthrough
const int   MODE       = 0;

const vec2  RESOLUTION = vec2(1600.0, 900.0);   // match your window size
const float NEAR       = 0.1;                    // match glm::perspective
const float FAR        = 100.0;

const vec3  SKY_TOP    = vec3(0.01, 0.02, 0.06);
const vec3  SKY_BOTTOM = vec3(0.10, 0.04, 0.18);
const vec3  LINE_COLOR = vec3(0.10, 0.95, 1.00);
const vec3  FOG_COLOR  = vec3(0.08, 0.05, 0.16);
// ----------------------------------------------------------------------------

// Depth buffer is [0,1] (GLM_FORCE_DEPTH_ZERO_TO_ONE) and non-linear: turn it into view-space distance
float linearizeDepth(float d)
{
    return (NEAR * FAR) / (FAR - d * (FAR - NEAR));
}

float vignette(vec2 uv)
{
    vec2 p = uv * (1.0 - uv.yx);
    return pow(clamp(p.x * p.y * 18.0, 0.0, 1.0), 0.25);
}

float scanlines(vec2 fragCoord)
{
    return 0.92 + 0.08 * sin(fragCoord.y * 3.14159);
}

void main()
{
    vec2  uv    = gl_FragCoord.xy / RESOLUTION;
    vec3  col   = subpassLoad(inputColor).rgb;
    float d     = subpassLoad(inputDepth).r;
    float z     = linearizeDepth(d);
    bool  isSky = d >= 0.99999;

    vec3 sky = mix(SKY_TOP, SKY_BOTTOM, uv.y);

    // Contour lines every 0.5 units of distance, anti-aliased with screen-space derivatives
    float bands   = z * 2.0;
    float bandW   = fwidth(bands) * 1.5;
    float contour = 1.0 - smoothstep(0.0, bandW, abs(fract(bands) - 0.5) - (0.5 - bandW));
    float fog     = smoothstep(4.0, 45.0, z);

    vec3 result;

    if (MODE == 0)          // Sonar scanner: dark geometry, glowing depth contours, fog
    {
        vec3 base  = col * 0.35;
        float glow = contour * (1.0 - fog);
        result = base + LINE_COLOR * glow * 1.4;
        result = mix(result, FOG_COLOR, fog * 0.85);
        if (isSky)
        {
            // faint horizon grid in the background
            float grid = step(0.985, fract(uv.y * 40.0)) * 0.25 * uv.y;
            result = sky + LINE_COLOR * grid;
        }
    }
    else if (MODE == 1)     // Toon: posterized colour, ink rim from depth bands, soft fog
    {
        vec3 poster = floor(col * 4.0 + 0.5) / 4.0;
        float ink   = contour * 0.6;
        result = mix(poster, vec3(0.02), ink);
        result = mix(result, sky, fog);
        if (isSky) result = sky;
    }
    else if (MODE == 2)     // Depth view: white = close, black = far
    {
        result = vec3(1.0 - clamp(z / 40.0, 0.0, 1.0));
    }
    else                    // Passthrough
    {
        result = col;
    }

    if (MODE != 2 && MODE != 3)
    {
        result *= vignette(uv) * scanlines(gl_FragCoord.xy);
        result  = pow(result, vec3(0.9));      // slight contrast lift
    }

    outColor = vec4(result, 1.0);
}