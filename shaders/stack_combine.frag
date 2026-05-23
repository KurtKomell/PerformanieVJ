#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_below;
layout(binding = 2) uniform sampler2D u_above;

vec3 blendNormal(vec3 a, vec3 b, float alphaB)
{
    return b * alphaB + a * (1.0 - alphaB);
}

void main()
{
    vec4 below = texture(u_below, v_uv);
    vec4 above = texture(u_above, v_uv);
    float a = clamp(above.a, 0.0, 1.0);
    vec3 rgb = blendNormal(below.rgb, above.rgb, a);
    fragColor = vec4(rgb, 1.0);
}
