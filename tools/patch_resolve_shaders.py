#!/usr/bin/env python3
"""Patch existing effect_*.frag files with Resolve effect cases."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SH = ROOT / "shaders"


def patch(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if old not in text:
        if new.split("\n", 1)[0] in text:
            print("skip (already patched):", label)
            return
        raise SystemExit(f"patch anchor not found for {label} in {path.name}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print("patched:", label)


# --- effect_blur.frag ---
patch(
    SH / "effect_blur.frag",
    "    } else {\n        // Box / Gaussian / fast — separable 1D passes (H then V).",
    """    } else if (id == 11) {
        // mosaic_blur (Resolve)
        float blend = ubuf.params.x;
        float strength = ubuf.params.y;
        float sz = max(ubuf.params.w, 0.02) * (0.015 + strength * 0.05);
        vec2 q = floor(uv / sz) * sz + sz * 0.5;
        rgb = mix(tex.rgb, pvjSampleRgb(u_tex, q), blend);
    } else if (id == 12) {
        // lens_blur (Resolve)
        float blend = ubuf.params.x;
        float strength = ubuf.params.y;
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.004 + strength * 0.02)
                  + pvjBlur9(u_tex, uv, vec2(0.0, 1.0), 0.004 + strength * 0.02);
        rgb = mix(tex.rgb, blur * 0.5, blend);
    } else if (id == 13) {
        // sharpen_edges (Resolve)
        float blend = ubuf.params.x;
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.004);
        rgb = mix(tex.rgb, clamp(tex.rgb + (tex.rgb - blur) * ubuf.params.y * 3.0, 0.0, 1.0), blend);
    } else if (id == 14) {
        // soften_sharpen (Resolve)
        float blend = ubuf.params.x;
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.006 + ubuf.params.y * 0.02);
        rgb = mix(tex.rgb, mix(tex.rgb, blur, ubuf.params.z), blend);
    } else {
        // Box / Gaussian / fast — separable 1D passes (H then V).""",
    "blur resolve cases",
)

# --- effect_color.frag ---
patch(
    SH / "effect_color.frag",
    "    if (id == 20) {",
    """    if (id >= 25) {
        float blend = ubuf.params.x;
        float s = ubuf.params.y;
        float d = ubuf.params.z;
        float sz = ubuf.params.w;
        vec3 fx = tex.rgb;
        if (id == 25) fx = pow(max(tex.rgb, vec3(0.0)), vec3(1.0 / max(0.55 + s * 0.35, 0.1)));
        else if (id == 26) fx = tex.rgb * vec3(1.0 + s * 0.2, 1.0, 1.0 - s * 0.15);
        else if (id == 27) fx = tex.rgb / max(vec3(0.2 + s * 0.8), 0.05);
        else if (id == 28) fx = mix(tex.rgb, tex.rgb.bgr, s);
        else if (id == 29) fx = mix(tex.rgb, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.003), s * 0.5);
        else if (id == 30) fx = (tex.rgb - 0.5) * (1.0 + s * 2.0) + 0.5;
        else if (id == 31) {
            fx = mix(tex.rgb, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.008 + s * 0.02), s);
            fx += (fx - tex.rgb) * d;
        } else if (id == 32) {
            float l = dot(tex.rgb, PVJ_LUMA);
            fx = mix(tex.rgb, pvjHsl2rgb(vec3(l + d * 0.2, 1.0, 0.5)), s);
        } else if (id == 33) {
            float flick = 0.85 + 0.15 * sin(pvjTime() * mix(2.0, 20.0, s));
            fx = tex.rgb * flick;
        } else if (id == 34) fx = clamp(tex.rgb, 0.0, 0.85 + sz * 0.15);
        else if (id == 35) fx = mix(tex.rgb, tex.rgb.grb, s);
        fragColor = vec4(clamp(mix(tex.rgb, fx, blend), 0.0, 1.0), tex.a);
        return;
    }

    if (id == 20) {""",
    "color resolve cases",
)

# --- effect_distort.frag ---
patch(
    SH / "effect_distort.frag",
    "    } else if (id == 16) { // cube — pseudo cubemap\n        vec3 d = normalize(vec3(p, 0.5));\n        p = d.xy / (abs(d.x) + abs(d.y) + abs(d.z) + 1e-5) * 0.5;\n    }\n    return pvjUvFromCentered(p);",
    """    } else if (id == 16) { // cube — pseudo cubemap
        vec3 d = normalize(vec3(p, 0.5));
        p = d.xy / (abs(d.x) + abs(d.y) + abs(d.z) + 1e-5) * 0.5;
    } else if (id == 17) { // dent
        float r = length(p);
        p *= 1.0 - ubuf.params.y * exp(-r * 8.0) * 0.3;
    } else if (id == 18) { // lens_distortion
        float r = length(p);
        p *= 1.0 + ubuf.params.y * r * r * sign(r);
    } else if (id == 19) { // ripples
        float r = length(p);
        p += normalize(p + 1e-5) * sin(r * 20.0 - t * 4.0) * ubuf.params.y * 0.03;
    } else if (id == 20) { // vortex
        float r = length(p);
        float a = atan(p.y, p.x) + ubuf.params.y * (1.0 - r) * 3.0;
        p = vec2(cos(a), sin(a)) * r;
    } else if (id == 21) { // warper
        p += vec2(pvjNoise(p * 5.0 + t), pvjNoise(p * 5.0 + t + 2.0)) * ubuf.params.y * 0.08;
    } else if (id == 22) { // waviness
        p.x += sin(p.y * 10.0 + t) * ubuf.params.y * 0.04;
        p.y += cos(p.x * 10.0 + t) * ubuf.params.z * 0.04;
    }
    return pvjUvFromCentered(p);""",
    "distort resolve warp cases",
)

patch(
    SH / "effect_distort.frag",
    "void main()\n{\n    vec2 uv = distortUv(v_uv, pvjEffectId());\n    fragColor = pvjSampleRgba(u_tex, uv);\n}",
    """void main()
{
    int id = pvjEffectId();
    vec4 orig = pvjSampleRgba(u_tex, v_uv);
    vec2 uv = distortUv(v_uv, id);
    vec3 rgb = pvjSampleRgba(u_tex, uv).rgb;
    if (id >= 17) {
        rgb = mix(orig.rgb, rgb, ubuf.params.x);
    }
    fragColor = vec4(rgb, orig.a);
}""",
    "distort resolve blend",
)

# --- effect_transform.frag ---
patch(
    SH / "effect_transform.frag",
    "    } else if (id == 0) { // transform\n        float s = max(ubuf.params.x, 0.01);",
    """    } else if (id == 13) { // camera_shake
        float s = ubuf.params.y;
        p += vec2(pvjNoise(vec2(t * 10.0)), pvjNoise(vec2(t * 10.0 + 1.0))) * s * 0.06 - 0.03;
    } else if (id == 14) { // video_collage
        vec2 tile = floor(p * mix(2.0, 6.0, ubuf.params.z) + 0.5);
        p = fract(p * mix(2.0, 4.0, ubuf.params.y)) - 0.5;
        p += sin(tile + t) * ubuf.params.y * 0.02;
    } else if (id == 0) { // transform
        float s = max(ubuf.params.x, 0.01);""",
    "transform resolve cases",
)

patch(
    SH / "effect_transform.frag",
    "void main()\n{\n    int id = pvjEffectId();\n    vec2 uv = transformUv(v_uv, id);\n    fragColor = pvjSampleRgba(u_tex, uv);\n}",
    """void main()
{
    int id = pvjEffectId();
    vec4 orig = pvjSampleRgba(u_tex, v_uv);
    vec2 uv = transformUv(v_uv, id);
    vec3 rgb = pvjSampleRgba(u_tex, uv).rgb;
    if (id >= 13) {
        rgb = mix(orig.rgb, rgb, ubuf.params.x);
    }
    fragColor = vec4(rgb, orig.a);
}""",
    "transform resolve blend",
)

# Need t variable in transformUv for camera_shake - add at start of transformUv
patch(
    SH / "effect_transform.frag",
    "vec2 transformUv(vec2 uv, int id)\n{\n    vec2 p = pvjUvCentered(uv);\n    float amount = ubuf.params.x;",
    """vec2 transformUv(vec2 uv, int id)
{
    vec2 p = pvjUvCentered(uv);
    float t = pvjTime();
    float amount = ubuf.params.x;""",
    "transform time var",
)

print("done")

# --- effect_generate.frag ---
patch(
    SH / "effect_generate.frag",
    "    } else if (id == 17) { // slit_scanner\n        float spd = ubuf.params.x * 0.1;\n        float band = fract(uv.y + t * spd);\n        rgb = mix(rgb, pvjSampleRgb(u_tex, vec2(uv.x, fract(uv.y + band))), amount);\n    }\n\n    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);",
    """    } else if (id == 17) { // slit_scanner
        float spd = ubuf.params.x * 0.1;
        float band = fract(uv.y + t * spd);
        rgb = mix(rgb, pvjSampleRgb(u_tex, vec2(uv.x, fract(uv.y + band))), amount);
    } else if (id >= 18) {
        float blend = ubuf.params.x;
        float s = ubuf.params.y;
        float d = ubuf.params.z;
        float sz = ubuf.params.w;
        vec3 fx = rgb;
        if (id == 18) fx = pvjHsl2rgb(vec3(d, s, 0.5));
        else if (id == 19) fx = mix(pvjHsl2rgb(vec3(0.0, 0.8, 0.5)), pvjHsl2rgb(vec3(0.66, 0.8, 0.5)), s);
        else if (id == 20) fx = mix(rgb, vec3(0.15, 0.18, 0.25), s);
        else if (id == 21) fx = mix(rgb, vec3(pvjNoise(uv * 200.0 + t)), s);
        else if (id == 22) {
            float h = fract(uv.x + uv.y + t * 0.2);
            fx = mix(rgb, pvjHsl2rgb(vec3(h, 1.0, 0.5)), s);
        } else if (id == 23) {
            vec2 c = pvjUvCentered(uv) * 2.0;
            float m = 0.0;
            for (int i = 0; i < 8; ++i) {
                m = m * m + c;
                c = vec2(c.x * c.x - c.y * c.y, 2.0 * c.x * c.y) + vec2(-0.7, 0.27);
            }
            fx = mix(rgb, vec3(smoothstep(2.0, 0.0, length(c))), s);
        } else if (id == 24) {
            float g = smoothstep(0.2, 0.8, 1.0 - uv.y);
            fx = mix(rgb, mix(vec3(0.4, 0.6, 1.0), vec3(0.9, 0.95, 1.0), g), s);
        }
        rgb = mix(tex.rgb, fx, blend);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);""",
    "generate resolve cases",
)

# --- effect_pattern.frag ---
patch(
    SH / "effect_pattern.frag",
    "    } else if (id == 4) { // dots\n        vec2 g = fract(uv * scale * 30.0) - 0.5;\n        pat = vec3(step(0.15, length(g)));\n    }\n\n    vec3 src = pvjSampleRgb(u_tex, uv);",
    """    } else if (id == 4) { // dots
        vec2 g = fract(uv * scale * 30.0) - 0.5;
        pat = vec3(step(0.15, length(g)));
    } else if (id == 5) { // grid (Resolve)
        vec2 g = abs(fract(uv * scale * mix(8.0, 32.0, ubuf.params.w)) - 0.5);
        pat = vec3(smoothstep(0.02, 0.0, min(g.x, g.y)));
        amount = ubuf.params.x;
    }

    vec3 src = pvjSampleRgb(u_tex, uv);""",
    "pattern grid",
)

# --- effect_utility.frag ---
patch(
    SH / "effect_utility.frag",
    "    } else if (id == 7) { // transform_3d\n        vec2 p = pvjUvCentered(uv);\n        float rx = ubuf.params.x, ry = ubuf.params.y;\n        p.x += sin(ry) * p.y * ubuf.params.w;\n        p.y += sin(rx) * p.x * ubuf.params.w;\n        rgb = pvjSampleRgb(u_tex, pvjUvFromCentered(p));\n    }\n\n    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a * (id == 6 ? ubuf.params.x : 1.0));",
    """    } else if (id == 7) { // transform_3d
        vec2 p = pvjUvCentered(uv);
        float rx = ubuf.params.x, ry = ubuf.params.y;
        p.x += sin(ry) * p.y * ubuf.params.w;
        p.y += sin(rx) * p.x * ubuf.params.w;
        rgb = pvjSampleRgb(u_tex, pvjUvFromCentered(p));
    } else if (id == 8) { // dctl stub (Resolve)
        vec3 fx = rgb * (1.0 + ubuf.params.y * 0.5);
        fx = pow(max(fx, vec3(0.0)), vec3(1.0 / max(0.5 + ubuf.params.z, 0.1)));
        rgb = mix(rgb, fx, ubuf.params.x);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a * (id == 6 ? ubuf.params.x : 1.0));""",
    "utility dctl",
)

# --- effect_key.frag ---
patch(
    SH / "effect_key.frag",
    "    if (ubuf.rotation.w < 0.5) {",
    """    int keyId = int(ubuf.rotation.w + 0.5);
    if (keyId >= 2) {
        float blend = ubuf.params.x;
        float th = ubuf.params.y;
        float soft = max(ubuf.params.z, 0.04);
        float glow = ubuf.params.w;
        vec3 c = clamp(tex.rgb, vec3(0.0), vec3(1.0));
        float metric = 0.0;
        if (keyId == 2) metric = length(c - vec3(th));
        else if (keyId == 3) {
            vec3 hsl = pvjRgb2hsl(c);
            metric = abs(hsl.x - th);
            metric = min(metric, 1.0 - metric);
        } else metric = dot(c, PVJ_LUMA);
        float kA = smoothstep(th - soft, th + soft, metric);
        if (keyId == 4) kA = 1.0 - kA;
        float alpha = mix(baseA, baseA * kA, blend);
        if (keyId == 4) {
            vec3 blur = pvjBlur9(u_tex, v_uv, vec2(1.0, 0.0), 0.004 + glow * 0.01);
            c = mix(c, blur, glow * 0.5);
        }
        fragColor = vec4(c, alpha);
        return;
    }

    if (ubuf.rotation.w < 0.5) {""",
    "key resolve cases",
)

# --- effect_stylize.frag ---
STYLIZE_RESOLVE = """
    } else if (id >= 24) {
        float blend = ubuf.params.x;
        float s = ubuf.params.y;
        float d = ubuf.params.z;
        float sz = ubuf.params.w;
        vec3 fx = rgb;
        if (id == 24) fx = floor(rgb * mix(4.0, 16.0, d)) / mix(4.0, 16.0, d);
        else if (id == 25) {
            float m = max(abs(pvjUvCentered(uv).x), abs(pvjUvCentered(uv).y));
            fx = mix(rgb, vec3(0.0), step(1.0 - sz * 0.2, m) * s);
        } else if (id == 26) {
            vec3 e = sobel(u_tex, uv, 2.0);
            fx = mix(rgb, vec3(dot(e, PVJ_LUMA)), s);
        } else if (id == 27) {
            vec3 acc = vec3(0.0);
            for (int i = -2; i <= 2; ++i) acc += pvjSampleRgb(u_tex, uv + vec2(float(i) * s * 0.003, 0.0));
            fx = acc / 5.0;
        } else if (id == 28) fx = floor(rgb * mix(6.0, 24.0, s)) / mix(6.0, 24.0, s);
        else if (id == 29) {
            rgb += (pvjNoise(uv * 300.0 + t) - 0.5) * s * 0.4;
            fx = rgb;
        } else if (id == 30) {
            float n = step(0.97, pvjNoise(vec2(floor(uv.y * 200.0), t)));
            fx = mix(rgb, rgb * 0.5, n * s);
        } else if (id == 31) {
            fx = floor(rgb * mix(4.0, 16.0, s)) / mix(4.0, 16.0, s);
        } else if (id == 32) fx = mix(rgb, rgb * (0.9 + pvjNoise(uv * 100.0) * 0.2), s);
        else if (id == 33) {
            float r = length(pvjUvCentered(uv));
            fx = mix(rgb, rgb, 1.0);
            fx *= 1.0 - smoothstep(0.35, 0.45, r) * s;
            fx = mix(fx, fx * 0.3, smoothstep(0.35, 0.45, r) * s);
        } else if (id == 34) {
            fx = rgb * 0.7;
            fx += vec3(0.0, 0.05, 0.0) * s;
            fx.r *= 0.9;
        } else if (id == 35) {
            float b = smoothstep(0.4, 0.6, length(pvjUvCentered(uv)));
            fx = mix(rgb, rgb * 0.2, b * s);
        } else if (id == 36) {
            float g = step(0.98, fract(sin(floor(uv.y * 400.0) + t * 20.0) * 43758.5453));
            fx = mix(rgb, vec3(pvjNoise(uv + t)), g * s);
        } else if (id == 37) {
            fx = mix(rgb, vec3(0.2, 0.8, 0.3), 0.15 * s);
            fx += vec3(0.0, 0.1, 0.0) * d;
        } else if (id == 38) {
            float v = length(pvjUvCentered(uv));
            fx = mix(rgb, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.003), smoothstep(0.2, 0.5, v) * s);
        } else if (id == 39) {
            vec2 p = pvjUvCentered(uv);
            p.x *= 1.0 + s * 0.3 * sign(p.x);
            fx = pvjSampleRgb(u_tex, pvjUvFromCentered(p));
        } else if (id == 40) {
            fx = rgb;
            fx = floor(fx * 8.0) / 8.0;
            fx *= 0.9;
        } else if (id == 41) {
            float r = length(pvjUvCentered(uv));
            fx = mix(rgb, rgb * 0.5, smoothstep(0.3, 0.5, r) * s);
            fx += vec3(0.05) * d;
        }
        rgb = mix(tex.rgb, fx, blend);
    """

patch(
    SH / "effect_stylize.frag",
    "    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);\n}",
    STYLIZE_RESOLVE + "\n    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);\n}",
    "stylize resolve cases",
)

print("all patches applied")
