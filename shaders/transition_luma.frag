#version 440

// Luma-based reveal: reveals B where luminance of B crosses the threshold driven by progress.

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_texA;
layout(binding = 2) uniform sampler2D u_texB;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 transition; // x = progress 0–1 (threshold center), y = softness in luma space
} ubuf;

const vec3 kLum = vec3(0.2126, 0.7152, 0.0722);

void main()
{
    vec4 ca = texture(u_texA, v_uv);
    vec4 cb = texture(u_texB, v_uv);
    float lb = dot(cb.rgb, kLum);
    float p = clamp(ubuf.transition.x, 0.0, 1.0);
    float soft = max(ubuf.transition.y, 0.02);
    float m = smoothstep(p - soft * 0.5, p + soft * 0.5, lb);
    fragColor = mix(ca, cb, m);
}
