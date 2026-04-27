#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
} ubuf;

void main()
{
    vec2 uv = v_uv;
    float amount = ubuf.params.x;
    float freq = max(0.0, ubuf.params.y);
    float speed = ubuf.params.z;
    float t = ubuf.scaleOffset.z + speed * 0.15;
    uv.x += sin((uv.y + t) * (2.0 + freq)) * amount * 0.03;
    uv.y += cos((uv.x - t) * (2.0 + freq)) * amount * 0.03;
    fragColor = texture(u_tex, clamp(uv, vec2(0.001), vec2(0.999)));
}
