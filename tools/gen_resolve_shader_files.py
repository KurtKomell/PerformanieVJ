#!/usr/bin/env python3
"""Create Resolve family shaders and patch existing effect_*.frag files."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"
COMMON = (SHADERS / "effect_common.glsl").read_text(encoding="utf-8")

HEADER = """#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
    vec4 params2;
} ubuf;

"""

RESOLVE_HELPERS = """
float pvjResolveBlend() { return ubuf.params.x; }
float pvjResolveStrength() { return ubuf.params.y; }
float pvjResolveDetail() { return ubuf.params.z; }
float pvjResolveSize() { return ubuf.params.w; }
vec3 pvjResolveMix(vec3 src, vec3 fx) { return mix(src, fx, pvjResolveBlend()); }
"""


def write_family_shader(name: str, body: str) -> None:
    path = SHADERS / name
    path.write_text(HEADER + COMMON + RESOLVE_HELPERS + body, encoding="utf-8", newline="\n")
    print("wrote", name)


write_family_shader(
    "effect_light.frag",
    """
void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 fx = src;
    int id = pvjEffectId();
    float s = pvjResolveStrength();
    float d = pvjResolveDetail();
    float sz = pvjResolveSize();
    float t = pvjTime();
    vec2 p = pvjUvCentered(uv);

    if (id == 0) { // aperture_diffraction
        float r = length(p);
        float spikes = abs(sin(atan(p.y, p.x) * 8.0 + t * 0.2)) * s;
        fx += vec3(1.0, 0.9, 0.7) * spikes * (1.0 - r) * d;
    } else if (id == 1) { // halation
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.01 + s * 0.03)
                  + pvjBlur9(u_tex, uv, vec2(0.0, 1.0), 0.01 + s * 0.03);
        blur *= 0.5;
        float bright = max(max(src.r, src.g), src.b);
        fx = src + blur * smoothstep(0.5, 1.0, bright) * s * 1.5;
    } else if (id == 2) { // lens_flare
        vec2 flarePos = vec2(0.65, 0.35);
        float dist = length(p - flarePos);
        fx += vec3(1.0, 0.8, 0.4) * exp(-dist * 12.0 / max(s, 0.05)) * s;
        fx += vec3(0.4, 0.6, 1.0) * exp(-dist * 30.0) * s * d;
    } else if (id == 3) { // lens_reflections
        vec2 rp = reflect(normalize(p + 1e-5), normalize(vec2(sin(t * 0.3), cos(t * 0.2))));
        fx = mix(src, pvjSampleRgb(u_tex, pvjUvFromCentered(p + rp * s * 0.15)), 0.4 + d * 0.4);
    } else if (id == 4) { // light_rays
        vec2 c = uv - 0.5;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 14; ++i) {
            acc += pvjSampleRgb(u_tex, uv - c * float(i) * 0.015 * s);
        }
        fx = mix(src, acc / 14.0, s * (0.5 + sz));
    }

    fragColor = vec4(clamp(pvjResolveMix(src, fx), 0.0, 1.0), tex.a);
}
""",
)

write_family_shader(
    "effect_revival.frag",
    """
void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 fx = src;
    int id = pvjEffectId();
    float s = pvjResolveStrength();
    float d = pvjResolveDetail();
    float sz = pvjResolveSize();
    float t = pvjTime();
    vec2 px = vec2(1.0 / 1024.0);

    if (id == 0) { // automatic_dirt_removal
        float n = pvjNoise(uv * 400.0);
        fx = mix(src, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.004), step(0.92, n) * s);
    } else if (id == 1) { // chromatic_aberration_removal
        vec2 off = vec2(s * 0.002, 0.0);
        vec3 avg = (pvjSampleRgb(u_tex, uv + off) + pvjSampleRgb(u_tex, uv - off) + src) / 3.0;
        fx = mix(src, avg, s);
    } else if (id == 2) { // dead_pixel_fixer
        vec3 mn = src, mx = src;
        for (int i = -1; i <= 1; ++i)
            for (int j = -1; j <= 1; ++j) {
                vec3 n = pvjSampleRgb(u_tex, uv + vec2(float(i), float(j)) * px);
                mn = min(mn, n);
                mx = max(mx, n);
            }
        float spike = step(0.35, length(mx - mn));
        fx = mix(src, (mn + mx) * 0.5, spike * s);
    } else if (id == 3) { // deband
        fx = floor(src * mix(32.0, 128.0, d)) / mix(32.0, 128.0, d);
        fx = mix(src, fx, s);
    } else if (id == 4) { // deflicker
        float l = dot(src, PVJ_LUMA);
        float avgL = dot(pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.02), PVJ_LUMA);
        fx = src * mix(1.0, avgL / max(l, 0.01), s * 0.5);
    } else if (id == 5) { // frame_replacer
        vec2 off = vec2(sin(t * 3.0), cos(t * 2.0)) * s * 0.002;
        fx = pvjSampleRgb(u_tex, uv + off);
    } else if (id == 6) { // beauty
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.003 + s * 0.008);
        fx = mix(src, blur, s * 0.7);
    } else if (id == 7) { // patch_replacer
        vec2 patchUv = uv + vec2(sz * 0.1, d * 0.1);
        fx = mix(src, pvjSampleRgb(u_tex, patchUv), s);
    }

    fragColor = vec4(clamp(pvjResolveMix(src, fx), 0.0, 1.0), tex.a);
}
""",
)

write_family_shader(
    "effect_temporal.frag",
    """
void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 fx = src;
    int id = pvjEffectId();
    float s = pvjResolveStrength();
    float d = pvjResolveDetail();
    float sz = pvjResolveSize();
    float t = pvjTime();

    if (id == 0) { // motion_trails
        vec2 vel = vec2(sin(t * 2.0), cos(t * 1.7)) * s * 0.02;
        fx = src;
        for (int i = 1; i <= 6; ++i) {
            fx += pvjSampleRgb(u_tex, uv - vel * float(i)) * (1.0 / float(i + 1));
        }
        fx /= 2.5;
    } else if (id == 1) { // smear
        vec2 dir = vec2(cos(d * 6.28), sin(d * 6.28)) * s * 0.03;
        fx = pvjSampleRgb(u_tex, uv + dir);
        fx = mix(fx, pvjSampleRgb(u_tex, uv - dir), 0.5);
    } else if (id == 2) { // stop_motion
        float frames = mix(4.0, 24.0, 1.0 - sz);
        float q = floor(t * frames * s * 3.0) / (frames * s * 3.0);
        vec2 off = vec2(pvjNoise(vec2(q)), pvjNoise(vec2(q + 1.0))) * 0.002 * d;
        fx = pvjSampleRgb(u_tex, uv + off);
    }

    fragColor = vec4(clamp(pvjResolveMix(src, fx), 0.0, 1.0), tex.a);
}
""",
)
