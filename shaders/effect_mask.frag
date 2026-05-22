#version 440

// Alpha mask:
// params.x = mask type (0 none, 1 rect, 2 circle, 3 soft edge, 4 ellipse, 5 custom radial)
// params.y = sizeX (0..1), params.z = sizeY (0..1), params.w = feather (0..1)

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
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
    vec2 center = vec2(0.5, 0.5);
    int mt = int(ubuf.params.x + 0.5);
    float sizeX = max(ubuf.params.y, 1e-4);
    float sizeY = max(ubuf.params.z, 1e-4);
    float smoothness = clamp(ubuf.params.w, 0.001, 0.5);

    float m = 1.0;
    vec2 p = uv - center;

    if (mt == MASK_NONE) {
        m = 1.0;
    } else if (mt == MASK_RECT) {
        vec2 h = abs(p) * 2.0;
        float dx = max(h.x / sizeX, h.y / sizeY);
        float edge = 1.0;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, dx);
    } else if (mt == MASK_CIRCLE) {
        float r = min(sizeX, sizeY);
        float d = length(p) * 2.0 / r;
        float edge = 1.0;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    } else if (mt == MASK_SOFT) {
        vec2 h = abs(p) * 2.0;
        float dx = max(h.x / sizeX, h.y / sizeY);
        float inner = 1.0;
        m = smoothstep(inner + smoothness, inner - smoothness, dx);
    } else if (mt == MASK_ELLIPSE) {
        float d = length(vec2((p.x * 2.0) / sizeX, (p.y * 2.0) / sizeY));
        float edge = 1.0;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    } else if (mt == MASK_CUSTOM) {
        float a = atan(p.y, p.x);
        float rays = 6.0;
        float wave = 0.5 + 0.5 * cos(a * rays);
        float r = min(sizeX, sizeY);
        float d = (length(p) * 2.0 / r) * mix(0.85, 1.15, wave);
        float edge = 1.0;
        m = 1.0 - smoothstep(edge - smoothness, edge + smoothness, d);
    }

    fragColor = vec4(tex.rgb, tex.a * clamp(m, 0.0, 1.0));
}
