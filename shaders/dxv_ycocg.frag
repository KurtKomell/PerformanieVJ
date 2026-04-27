#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

// After hardware BC3 decode, channels match FFmpeg's dxt5_block_internal output
// before ycocg2rgba (scaled): Co in R, Cg in G, scale in B, Y in A — see
// libavcodec/texturedsp.c dxt5ys_block + ycocg2rgba.

void main()
{
    vec4 t = texture(u_tex, v_uv);
    float r = t.r * 255.0;
    float g = t.g * 255.0;
    float b = t.b * 255.0;
    float a = t.a * 255.0;

    int ib = int(floor(b + 0.5));
    float s = float(ib >> 3) + 1.0;
    float Y = a;
    float co = (r - 128.0) / s;
    float cg = (g - 128.0) / s;
    float R = clamp(Y + co - cg, 0.0, 255.0) / 255.0;
    float G = clamp(Y + cg, 0.0, 255.0) / 255.0;
    float B = clamp(Y - co - cg, 0.0, 255.0) / 255.0;
    fragColor = vec4(R, G, B, 1.0);
}
