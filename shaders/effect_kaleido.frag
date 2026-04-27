#version 440

// Kaleidoscope: fold UV into a sector. params.x = segment count (>=2),
// params.y = angular phase offset (radians), params.z = mirror (0/1).

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
} ubuf;

const float PI = 3.14159265359;

void main()
{
    vec2 uv = v_uv - 0.5;
    float seg = max(ubuf.params.x, 2.0);
    float phase = ubuf.params.y;
    float mirror = ubuf.params.z > 0.5 ? 1.0 : 0.0;

    float r = length(uv);
    float a = atan(uv.y, uv.x) + phase;

    float sector = PI * 2.0 / seg;
    a = mod(a, sector);
    if (mirror > 0.5) {
        a = abs(a - sector * 0.5);
    }

    vec2 uv2 = vec2(cos(a), sin(a)) * r + 0.5;
    uv2 = clamp(uv2, vec2(0.001), vec2(0.999));

    fragColor = texture(u_tex, uv2);
}
