#version 440

// Alpha mask: params.x = mask type (0 none, 1 rect, 2 circle, 3 soft edge, 4 ellipse, 5 custom radial).
// params.y = width / size parameter (0–1), params.z = edge softness (0–1).
// params2.xy = center in UV (0–1), default (0.5, 0.5).

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
    vec4 params2;
} ubuf;

const int MASK_NONE = 0;
const int MASK_RECT = 1;
const int MASK_CIRCLE = 2;
const int MASK_SOFT = 3;
const int MASK_ELLIPSE = 4;
const int MASK_CUSTOM = 5;

void main()
{
    vec4 tex = texture(u_tex, v_uv);
    vec2 uv = v_uv;
    vec2 center = ubuf.params2.xy;
    int mt = int(ubuf.params.x + 0.5);
    float size = max(ubuf.params.y, 1e-4);
    float smoothness = clamp(ubuf.params.z, 0.001, 0.5);

    float m = 1.0;
    vec2 p = uv - center;

    if (mt == MASK_NONE) {
        m = 1.0;
    } else if (mt == MASK_RECT) {
        vec2 h = abs(p) * 2.0;
        float edge = 1.0 - size;
        float dx = max(h.x, h.y);
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, dx);
    } else if (mt == MASK_CIRCLE) {
        float d = length(p) * 2.0;
        float edge = 1.0 - size;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    } else if (mt == MASK_SOFT) {
        vec2 h = abs(p) * 2.0;
        float dx = max(h.x, h.y);
        float inner = 1.0 - size;
        m = smoothstep(inner + smoothness, inner - smoothness, dx);
    } else if (mt == MASK_ELLIPSE) {
        vec2 scale = vec2(1.0 + size, 1.0);
        float d = length(p * scale) * 2.0;
        float edge = 1.0 - size * 0.5;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    } else if (mt == MASK_CUSTOM) {
        float a = atan(p.y, p.x);
        float rays = 6.0;
        float wave = 0.5 + 0.5 * cos(a * rays);
        float d = length(p) * 2.0 * mix(0.85, 1.15, wave);
        float edge = 1.0 - size;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    }

    fragColor = vec4(tex.rgb, tex.a * clamp(m, 0.0, 1.0));
}
