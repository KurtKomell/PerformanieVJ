#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_current;
layout(binding = 2) uniform sampler2D u_history;

layout(std140, binding = 0) uniform Block {
    vec4 fbA; // x=strength, y=decay, z=zoom, w=rotationRad
    vec4 fbB; // x=brightness, y=saturation, z=gamma, w=contrast (history)
    vec4 fbC; // x=layerBrightness, y=layerSaturation, z=layerGamma, w=layerContrast
    vec4 fbD; // padding (reserved)
} ubuf;

vec3 applySaturation(vec3 c, float sat)
{
    const vec3 lumaW = vec3(0.2126, 0.7152, 0.0722);
    float l = dot(c, lumaW);
    return mix(vec3(l), c, sat);
}

vec3 applyContrast(vec3 c, float contrast)
{
    return (c - 0.5) * contrast + 0.5;
}

void main()
{
    float strength = clamp(ubuf.fbA.x, 0.0, 1.0);
    float decay = ubuf.fbA.y;
    float zoom = clamp(ubuf.fbA.z, 0.5, 2.0);
    float rot = ubuf.fbA.w;

    vec2 p = v_uv - vec2(0.5);
    mat2 r = mat2(cos(rot), -sin(rot), sin(rot), cos(rot));
    vec2 histUv = (r * (p / zoom)) + vec2(0.5);

    vec3 cur = texture(u_current, v_uv).rgb;
    vec3 hist = texture(u_history, histUv).rgb;

    hist = hist + decay + vec3(ubuf.fbB.x);
    hist = applySaturation(hist, clamp(ubuf.fbB.y, 0.0, 2.0));
    hist = applyContrast(hist, clamp(ubuf.fbB.w, 0.0, 2.0));
    hist = pow(max(hist, vec3(0.0)), vec3(1.0 / max(ubuf.fbB.z, 0.001)));

    vec3 outRgb = mix(cur, hist, strength);

    float lb = ubuf.fbC.x;
    float lsat = clamp(ubuf.fbC.y, 0.0, 2.0);
    float lgamma = max(ubuf.fbC.z, 0.001);
    float lcon = clamp(ubuf.fbC.w, 0.0, 2.0);
    outRgb = outRgb + vec3(lb);
    outRgb = applySaturation(outRgb, lsat);
    outRgb = applyContrast(outRgb, lcon);
    outRgb = pow(max(outRgb, vec3(0.0)), vec3(1.0 / lgamma));

    fragColor = vec4(clamp(outRgb, 0.0, 1.0), 1.0);
}
