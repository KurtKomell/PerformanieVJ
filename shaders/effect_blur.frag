#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
    vec4 params2;
    vec4 params3;
} ubuf;

const float PVJ_PI = 3.14159265359;

int pvjEffectId() { return int(ubuf.rotation.w + 0.5); }
int pvjPass() { return int(ubuf.params2.x + 0.5); }

// params2.y: blur_type (0 = Realistic, 1 = Stylized); rotation.x is vertex rotation only.
bool pvjBlurRealistic() { return ubuf.params2.y < 0.5; }

vec3 pvjSampleRgb(vec2 uv)
{
    return texture(u_tex, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
}

vec4 pvjSampleRgba(vec2 uv)
{
    return texture(u_tex, clamp(uv, vec2(0.001), vec2(0.999)));
}

// Realistic: Gaussian-like falloff (photographic). Stylized: uniform (digital smooth).
float pvjMotionWeight(float t, bool realistic)
{
    if (!realistic) {
        return 1.0;
    }
    return exp(-3.0 * t * t);
}

vec3 pvjBlurSeparable1D(vec2 uv, vec2 dir, float rad)
{
    vec3 c = pvjSampleRgb(uv) * 0.227027;
    c += pvjSampleRgb(uv + dir * rad) * 0.316216;
    c += pvjSampleRgb(uv - dir * rad) * 0.316216;
    c += pvjSampleRgb(uv + dir * rad * 2.0) * 0.070270;
    c += pvjSampleRgb(uv - dir * rad * 2.0) * 0.070270;
    return c;
}

vec3 pvjBlur9(vec2 uv, vec2 dir, float r)
{
    vec2 o = dir * r;
    const float w00 = 0.0625; const float w01 = 0.125; const float w02 = 0.0625;
    const float w10 = 0.125;  const float w11 = 0.25;  const float w12 = 0.125;
    const float w20 = 0.0625; const float w21 = 0.125; const float w22 = 0.0625;
    return pvjSampleRgb(uv + vec2(-o.x, -o.y)) * w00
         + pvjSampleRgb(uv + vec2(0.0, -o.y)) * w01
         + pvjSampleRgb(uv + vec2(o.x, -o.y)) * w02
         + pvjSampleRgb(uv + vec2(-o.x, 0.0)) * w10
         + pvjSampleRgb(uv + vec2(0.0, 0.0)) * w11
         + pvjSampleRgb(uv + vec2(o.x, 0.0)) * w12
         + pvjSampleRgb(uv + vec2(-o.x, o.y)) * w20
         + pvjSampleRgb(uv + vec2(0.0, o.y)) * w21
         + pvjSampleRgb(uv + vec2(o.x, o.y)) * w22;
}

// Gaussian: always smooth symmetrical blur (Resolve has no Blur Type on Gaussian).
vec3 pvjGaussianBlur(vec2 uv, vec4 tex, float hRad, float vRad, float blend)
{
    vec3 blur = pvjBlur9(uv, vec2(1.0, 0.0), hRad) * 0.5 + pvjBlur9(uv, vec2(0.0, 1.0), vRad) * 0.5;
    return mix(tex.rgb, blur, blend);
}

// Box: separable H×V in one pass (fixes broken horizontal-only 2-pass path).
vec3 pvjBoxBlur(vec2 uv, vec4 tex, float hRad, float vRad, float blend, bool realistic)
{
    if (hRad <= 0.0 && vRad <= 0.0) {
        return tex.rgb;
    }

    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int j = -2; j <= 2; ++j) {
        for (int i = -2; i <= 2; ++i) {
            if (hRad <= 0.0 && i != 0) {
                continue;
            }
            if (vRad <= 0.0 && j != 0) {
                continue;
            }
            vec2 off = vec2(float(i) * hRad, float(j) * vRad);
            float w = 1.0;
            if (realistic) {
                w = exp(-0.5 * (float(i * i) + float(j * j)));
            }
            acc += pvjSampleRgb(uv + off) * w;
            wsum += w;
        }
    }
    return mix(tex.rgb, acc / max(wsum, 1e-5), blend);
}

// Directional: Realistic = photographic motion blur; Stylized = uniform streak.
vec3 pvjDirectionalBlur(vec2 uv, vec4 tex, float strength, float ang, bool symmetric, bool realistic,
                        float blend)
{
    vec2 dir = vec2(cos(ang), sin(ang));
    const int taps = realistic ? 24 : 12;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < taps; ++i) {
        float u = float(i) / float(taps - 1);
        float offset;
        float w;
        if (symmetric) {
            offset = (u * 2.0 - 1.0) * strength * 18.0;
            w = pvjMotionWeight(u * 2.0 - 1.0, realistic);
        } else {
            offset = u * strength * 18.0;
            w = realistic ? exp(-4.0 * u) : 1.0;
        }
        acc += pvjSampleRgb(uv + dir * offset) * w;
        wsum += w;
    }
    vec3 blurred = acc / max(wsum, 1e-5);
    return mix(tex.rgb, blurred, blend);
}

// Radial (spin): samples along tangent; Realistic = weighted, Stylized = uniform.
vec3 pvjRadialBlur(vec2 uv, vec4 tex, float strength, float symmetry, bool realistic, float blend)
{
    vec2 c = uv - 0.5;
    float radius = length(c);
    vec2 tangent = radius > 1e-5 ? vec2(-c.y, c.x) / radius : vec2(0.0, 1.0);
    const int taps = realistic ? 20 : 10;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < taps; ++i) {
        float u = float(i) / float(taps - 1);
        float t;
        if (symmetry < 0.5) {
            t = u * 2.0 - 1.0;
        } else if (symmetry < 1.5) {
            t = u;
        } else {
            t = -u;
        }
        float offset = t * strength * 14.0 * max(radius, 0.02);
        float w = realistic ? pvjMotionWeight(t, true) : 1.0;
        acc += pvjSampleRgb(uv + tangent * offset) * w;
        wsum += w;
    }
    return mix(tex.rgb, acc / max(wsum, 1e-5), blend);
}

// Mosaic: aliasing 1 = tight point sample, 0 = average over full cell (Resolve manual).
vec3 pvjMosaicBlur(vec2 uv, vec4 tex, float freq, float cellShape, float aliasing, float blend)
{
    float sz = 1.0 / max(freq, 1.0);
    vec2 cellId = floor(uv / sz);
    vec2 cellMin = cellId * sz;
    vec2 cellMax = cellMin + sz;
    vec2 cellCenter = cellMin + sz * 0.5;

    float tight = clamp(aliasing, 0.0, 1.0);
    float sampleRadius = (1.0 - tight) * sz * 0.5;

    vec3 cellColor;
    if (sampleRadius <= sz * 0.0005) {
        cellColor = pvjSampleRgb(cellCenter);
    } else {
        vec3 acc = vec3(0.0);
        float wsum = 0.0;
        const int taps = 7;
        for (int j = 0; j < taps; ++j) {
            for (int i = 0; i < taps; ++i) {
                float fu = float(i) / float(taps - 1) * 2.0 - 1.0;
                float fv = float(j) / float(taps - 1) * 2.0 - 1.0;
                vec2 suv = cellCenter + vec2(fu, fv) * sampleRadius;
                suv = clamp(suv, cellMin + vec2(0.001), cellMax - vec2(0.001));
                acc += pvjSampleRgb(suv);
                wsum += 1.0;
            }
        }
        cellColor = acc / max(wsum, 1.0);
    }

    // cellShape reserved: 0 square, 1 hexagon, 2 triangle (square grid for now).
    return mix(tex.rgb, cellColor, blend);
}

// Zoom: Realistic = bidirectional zoom_amount (0 = no blur); Stylized = smooth_strength only.
vec3 pvjZoomBlur(vec2 uv, vec4 tex, float zoomAmount, float smoothStrength, float centerExcl,
                 bool realistic, float blend)
{
    vec2 c = uv - 0.5;
    float dist = length(c);
    if (dist < centerExcl * 0.5) {
        return tex.rgb;
    }
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    const int taps = realistic ? 20 : 12;
    for (int i = 0; i < taps; ++i) {
        float u = float(i) / float(taps - 1);
        float scale;
        float w;
        if (realistic) {
            float signedAmt = (zoomAmount - 0.5) * 2.0;
            scale = 1.0 - u * signedAmt * 0.45;
            w = pvjMotionWeight(u * 2.0 - 1.0, true);
        } else {
            scale = 1.0 - u * smoothStrength * 0.45;
            w = 1.0;
        }
        scale = clamp(scale, 0.05, 2.0);
        acc += pvjSampleRgb(c * scale + 0.5) * w;
        wsum += w;
    }
    return mix(tex.rgb, acc / max(wsum, 1e-5), blend);
}

float pvjLuma(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 pvjBlurSeparable(vec2 uv, float rad)
{
    return pvjBlur9(uv, vec2(1.0, 0.0), rad) * 0.5 + pvjBlur9(uv, vec2(0.0, 1.0), rad) * 0.5;
}

// ResolveFX Sharpen: multi-scale unsharp on fine / medium / large detail bands.
vec3 pvjSharpen(vec2 uv, vec3 tex, float amount, float fineSize, float fineW, float medW, float largeW,
                float blend)
{
    const float strength = amount / 5.0;
    const float fineR = 0.002 + fineSize * 0.02;
    const float medR = fineR * 2.5 + 0.004;
    const float largeR = fineR * 6.0 + 0.01;
    vec3 result = tex;

    if (fineW > 0.001) {
        vec3 blur = pvjBlurSeparable(uv, fineR);
        result += (tex - blur) * fineW * strength * 0.35;
    }
    if (medW > 0.001) {
        vec3 blur = pvjBlurSeparable(uv, medR);
        result += (tex - blur) * medW * strength * 0.28;
    }
    if (largeW > 0.001) {
        vec3 blur = pvjBlurSeparable(uv, largeR);
        result += (tex - blur) * largeW * strength * 0.22;
    }
    return mix(tex, clamp(result, 0.0, 1.0), blend);
}

float pvjLocalEdge(vec2 uv, float texel)
{
    const float l = pvjLuma(pvjSampleRgb(uv));
    const float lx = pvjLuma(pvjSampleRgb(uv + vec2(texel, 0.0)));
    const float ly = pvjLuma(pvjSampleRgb(uv + vec2(0.0, texel)));
    const float lxm = pvjLuma(pvjSampleRgb(uv - vec2(texel, 0.0)));
    const float lym = pvjLuma(pvjSampleRgb(uv - vec2(0.0, texel)));
    return length(vec2(lx - lxm, ly - lym));
}

// ResolveFX Sharpen Edges: edge-keyed unsharp mask.
vec3 pvjSharpenEdges(vec2 uv, vec3 tex, float amount, float radius, float threshold, float maskStrength,
                     float edgeBlur, float preDenoise, bool displayEdges, float texel, float blend)
{
    float edge = pvjLocalEdge(uv, texel);
    if (preDenoise > 0.001) {
        float avg = edge;
        avg += pvjLocalEdge(uv + vec2(texel, 0.0), texel);
        avg += pvjLocalEdge(uv - vec2(texel, 0.0), texel);
        avg += pvjLocalEdge(uv + vec2(0.0, texel), texel);
        avg += pvjLocalEdge(uv - vec2(0.0, texel), texel);
        edge = mix(edge, avg * 0.2, preDenoise);
    }

    const float t = threshold * 0.35;
    const float soft = 0.08 + edgeBlur * 0.25;
    float mask = smoothstep(t, t + soft, edge) * maskStrength;
    mask = clamp(mask, 0.0, 1.0);

    if (displayEdges) {
        return vec3(mask);
    }

    const float rad = 0.003 + radius * 0.03;
    const vec3 blur = pvjBlurSeparable(uv, rad);
    const vec3 detail = tex - blur;
    const vec3 sharpened = tex + detail * amount * mask * 2.0;
    return mix(tex, clamp(sharpened, 0.0, 1.0), blend);
}

vec3 pvjApplyTextureBand(vec2 uv, vec3 tex, vec3 accum, float rad, float textureVal, float coring)
{
    if (abs(textureVal) < 0.001) {
        return accum;
    }
    const vec3 blur = pvjBlurSeparable(uv, rad);
    const vec3 detail = tex - blur;
    float core = 1.0;
    if (coring > 0.001) {
        core = smoothstep(0.0, coring * 0.15 + 0.001, length(detail));
    }
    return accum + detail * textureVal * core * 0.45;
}

// ResolveFX Soften & Sharpen: per-band soften (negative) or sharpen (positive).
vec3 pvjSoftenSharpen(vec2 uv, vec3 tex, float smallT, float medT, float largeT, float smallSize,
                      float coring, float blend)
{
    const float fineR = 0.002 + smallSize * 0.012;
    const float medR = fineR * 3.0 + 0.005;
    const float largeR = fineR * 8.0 + 0.012;
    vec3 result = tex;
    result = pvjApplyTextureBand(uv, tex, result, fineR, smallT, coring);
    result = pvjApplyTextureBand(uv, tex, result, medR, medT, coring);
    result = pvjApplyTextureBand(uv, tex, result, largeR, largeT, coring);
    return mix(tex, clamp(result, 0.0, 1.0), blend);
}

void main()
{
    vec2 uv = v_uv;
    const float texel = max(ubuf.params2.z, 1.0 / 4096.0);
    const float hRad = max(ubuf.params.x, texel * 2.0);
    const float vRad = max(ubuf.params.y, texel * 2.0);
    const float blend = ubuf.params.w;
    const bool realistic = pvjBlurRealistic();
    const int id = pvjEffectId();
    vec4 tex = pvjSampleRgba(uv);
    vec3 rgb = tex.rgb;

    if (id == 0) {
        rgb = pvjGaussianBlur(uv, tex, hRad, vRad, blend);
    } else if (id == 1) {
        rgb = pvjBoxBlur(uv, tex, ubuf.params.x, ubuf.params.y, blend, realistic);
    } else if (id == 2) {
        rgb = pvjDirectionalBlur(uv, tex, hRad, ubuf.params.z, ubuf.rotation.z > 0.5, realistic, blend);
    } else if (id == 3) {
        rgb = pvjRadialBlur(uv, tex, hRad, ubuf.params.z, realistic, blend);
    } else if (id == 4) {
        rgb = pvjZoomBlur(uv, tex, ubuf.params.x, ubuf.params.y, ubuf.params.z, realistic, blend);
    } else if (id == 5) {
        rgb = pvjMosaicBlur(uv, tex, max(ubuf.params.x, 1.0), ubuf.params.y, ubuf.params.z, blend);
    } else if (id == 6) {
        float size = max(ubuf.params.x, 0.01);
        float hi = ubuf.params.y;
        vec3 blur = pvjBlur9(uv, vec2(1.0, 0.0), size * 0.02) * 0.5
                  + pvjBlur9(uv, vec2(0.0, 1.0), size * 0.02) * 0.5;
        rgb = mix(tex.rgb, blur, blend * (0.5 + hi * 0.5));
    } else if (id == 7) {
        rgb = pvjSharpen(uv, tex.rgb, ubuf.params.x, ubuf.params.y, ubuf.params.z, ubuf.params2.y,
                         ubuf.params2.w, blend);
    } else if (id == 8) {
        rgb = pvjSharpenEdges(uv, tex.rgb, ubuf.params.x, ubuf.params.y, ubuf.params.z,
                              ubuf.params2.y, ubuf.params2.w, ubuf.params3.x, ubuf.params3.y > 0.5,
                              texel, blend);
    } else if (id == 9) {
        rgb = pvjSoftenSharpen(uv, tex.rgb, ubuf.params.x, ubuf.params.y, ubuf.params.z,
                               ubuf.params2.y, ubuf.params2.w, blend);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
