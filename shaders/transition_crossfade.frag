#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_texA;
layout(binding = 2) uniform sampler2D u_texB;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 transition;
} ubuf;

void main()
{
    float t = clamp(ubuf.transition.x, 0.0, 1.0);
    vec4 a = texture(u_texA, v_uv);
    vec4 b = texture(u_texB, v_uv);
    fragColor = mix(a, b, t);
}
