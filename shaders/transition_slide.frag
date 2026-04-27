#version 440

// Slide: B enters along direction transition.zw (normalized). progress in transition.x.

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
    vec2 dir = ubuf.transition.zw;
    float len = length(dir);
    if (len < 1e-4) {
        dir = vec2(1.0, 0.0);
    } else {
        dir /= len;
    }

    // Perpendicular coordinate: wipe reveals B as t goes 0→1
    float d = dot(v_uv - 0.5, vec2(-dir.y, dir.x));
    float m = smoothstep(-0.02, 0.02, d + (t - 0.5) * 2.0);

    vec4 a = texture(u_texA, v_uv);
    vec4 b = texture(u_texB, v_uv);
    fragColor = mix(a, b, m);
}
