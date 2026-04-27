#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;
layout(binding = 2) uniform sampler2D u_tex_b;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
} ubuf;

vec3 blendOverlay(vec3 a, vec3 b)
{
    vec3 low = 2.0 * a * b;
    vec3 high = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
    return mix(low, high, step(0.5, a));
}

void main()
{
    vec4 a = texture(u_tex, v_uv);
    vec4 b = texture(u_tex_b, v_uv);
    float mode = round(ubuf.params.x);
    float mixAmount = clamp(ubuf.params.y, 0.0, 1.0);

    vec3 outRgb = a.rgb;
    if (mode < 0.5) {
        outRgb = b.rgb;
    } else if (mode < 1.5) {
        outRgb = a.rgb + b.rgb;
    } else if (mode < 2.5) {
        outRgb = max(a.rgb - b.rgb, vec3(0.0));
    } else if (mode < 3.5) {
        outRgb = a.rgb * b.rgb;
    } else if (mode < 4.5) {
        outRgb = 1.0 - (1.0 - a.rgb) * (1.0 - b.rgb);
    } else if (mode < 5.5) {
        outRgb = blendOverlay(a.rgb, b.rgb);
    } else if (mode < 6.5) {
        outRgb = abs(a.rgb - b.rgb);
    } else {
        outRgb = a.rgb + b.rgb - 2.0 * a.rgb * b.rgb;
    }
    fragColor = vec4(mix(a.rgb, outRgb, mixAmount), max(a.a, b.a));
}
