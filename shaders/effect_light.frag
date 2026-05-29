#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;
layout(binding = 2) uniform sampler2D u_orig;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
    vec4 params2;
    vec4 params3;
    vec4 light[16];
} ubuf;

const float PVJ_PI = 3.14159265359;
const vec3 PVJ_LUMA = vec3(0.2126, 0.7152, 0.0722);

int pvjEffectId() { return int(ubuf.rotation.w + 0.5); }
float pvjTime() { return ubuf.scaleOffset.z; }
float pvjAspect() { return max(ubuf.scaleOffset.w, 0.001); }
int pvjPass() { return int(ubuf.params2.x + 0.5); }

vec2 pvjUvCentered(vec2 uv) { return uv - 0.5; }

float pvjLuma(vec3 c) { return dot(c, PVJ_LUMA); }

vec3 pvjSampleRgb(sampler2D tex, vec2 uv)
{
    return texture(tex, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
}

vec4 pvjSampleRgba(sampler2D tex, vec2 uv)
{
    return texture(tex, clamp(uv, vec2(0.001), vec2(0.999)));
}

vec3 pvjBlur9(sampler2D tex, vec2 uv, vec2 dir, float r)
{
    vec2 o = dir * r;
    const float w00 = 0.0625; const float w01 = 0.125; const float w02 = 0.0625;
    const float w10 = 0.125;  const float w11 = 0.25;  const float w12 = 0.125;
    const float w20 = 0.0625; const float w21 = 0.125; const float w22 = 0.0625;
    return pvjSampleRgb(tex, uv + vec2(-o.x, -o.y)) * w00
         + pvjSampleRgb(tex, uv + vec2(0.0, -o.y)) * w01
         + pvjSampleRgb(tex, uv + vec2(o.x, -o.y)) * w02
         + pvjSampleRgb(tex, uv + vec2(-o.x, 0.0)) * w10
         + pvjSampleRgb(tex, uv + vec2(0.0, 0.0)) * w11
         + pvjSampleRgb(tex, uv + vec2(o.x, 0.0)) * w12
         + pvjSampleRgb(tex, uv + vec2(-o.x, o.y)) * w20
         + pvjSampleRgb(tex, uv + vec2(0.0, o.y)) * w21
         + pvjSampleRgb(tex, uv + vec2(o.x, o.y)) * w22;
}

vec3 pvjBlendAdd(vec3 a, vec3 b) { return a + b; }
vec3 pvjBlendScreen(vec3 a, vec3 b) { return 1.0 - (1.0 - a) * (1.0 - b); }
vec3 pvjBlendOverlay(vec3 a, vec3 b)
{
    vec3 low = 2.0 * a * b;
    vec3 high = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
    return mix(low, high, step(0.5, a));
}

vec3 pvjComposite(vec3 base, vec3 fx, int mode)
{
    if (mode == 0) return pvjBlendAdd(base, fx);
    if (mode == 1) return pvjBlendScreen(base, fx);
    if (mode == 2) return pvjBlendOverlay(base, fx);
    return mix(base, fx, pvjLuma(fx));
}

float pvjStar(vec2 p, int blades, float angle, float curvature)
{
    float a = atan(p.y, p.x) + angle;
    float spikes = float(blades);
    float v = abs(cos(a * spikes * 0.5));
    v = mix(v, pow(v, 1.0 + curvature * 4.0), 0.6);
    float r = length(p);
    return pow(max(0.0, 1.0 - r * 2.5), 2.0 + ubuf.params.z * 4.0) * v;
}

vec3 pvjApertureDiffraction(vec2 uv, vec3 src)
{
    float thresh = ubuf.params.y;
    float luma = pvjLuma(src);
    if (luma < thresh) {
        return src;
    }
    vec2 p = pvjUvCentered(uv);
    p.x *= mix(1.0, pvjAspect(), ubuf.light[1].x);
    float rot = ubuf.light[0].w * PVJ_PI / 180.0;
    float c = cos(rot);
    float s = sin(rot);
    p = vec2(c * p.x - s * p.y, s * p.x + c * p.y);

    int blades = int(ubuf.light[0].x + 0.5) + 3;
    float star = pvjStar(p, blades, rot, ubuf.light[0].z);
    float chroma = ubuf.light[1].y;
    vec3 tint = vec3(1.0, 0.92 - chroma * 0.15, 0.85 - chroma * 0.25);
    vec3 fx = tint * star * ubuf.params.w * (src * (1.0 + ubuf.light[0].y));
    return mix(src, src + fx, ubuf.params.x);
}

vec3 pvjThresholdPass(vec2 uv, float thresh, bool useAlpha)
{
    vec4 tex = pvjSampleRgba(u_orig, uv);
    vec3 src = tex.rgb;
    float m = pvjLuma(src);
    if (useAlpha) {
        m = max(m, tex.a);
    }
    const float high = min(thresh + 0.22, 1.0);
    float mask = smoothstep(thresh, high, m);
    vec3 highlight = max(src - vec3(thresh * 0.5), vec3(0.0));
    return highlight * mask;
}

vec3 pvjBlurPass(vec2 uv, vec2 dir, float radius)
{
    return pvjBlur9(u_tex, uv, dir, radius);
}

vec3 pvjCompositeOrig(vec2 uv, vec3 fx, float blend, int compMode)
{
    vec3 orig = pvjSampleRgb(u_orig, uv);
    vec3 outRgb = pvjComposite(orig, fx, compMode);
    return mix(orig, outRgb, blend);
}

vec3 pvjGlow(vec2 uv)
{
    const int pass = pvjPass();
    const float blend = ubuf.params.x;
    const float thresh = ubuf.params.y;
    const float blurH = max(ubuf.params.z, 0.001);
    const float blurV = max(ubuf.params.w, 0.001);
    const float brightness = ubuf.light[0].x;
    const int compMode = int(ubuf.light[0].y + 0.5);
    const bool useAlpha = ubuf.light[0].z > 0.5;
    const vec3 glowColor = ubuf.light[1].rgb;
    const float gamma = max(ubuf.light[1].w, 0.01);

    if (pass == 0) {
        return pvjThresholdPass(uv, thresh, useAlpha);
    }
    if (pass == 1) {
        return pvjBlurPass(uv, vec2(1.0 / pvjAspect(), 0.0), blurH);
    }
    if (pass == 2) {
        return pvjBlurPass(uv, vec2(0.0, 1.0), blurV);
    }
    vec3 blurred = pvjSampleRgb(u_tex, uv) * brightness;
    blurred = pow(max(blurred, vec3(0.0)), vec3(1.0 / max(gamma, 0.05)));
    blurred *= glowColor;
    return pvjCompositeOrig(uv, blurred, blend, compMode);
}

// Resolve-style halation: blur first, then threshold on blurred luma (matches film_halation logic).
vec3 pvjHalation(vec2 uv)
{
    const float blend = ubuf.params.x;
    const float thresh = ubuf.params.y;
    const float spread = clamp(ubuf.params.z, 0.0, 1.0);
    const float strength = ubuf.light[0].x;
    const float filmSat = ubuf.light[0].z;
    const vec3 halationColor = ubuf.light[1].rgb;
    const float gamma = ubuf.light[1].w;

  // Spread controls blur radius (0.004 .. 0.14 UV — clearly visible across the slider).
    const float blurR = 0.004 + spread * 0.136;
    vec3 blur = pvjBlur9(u_orig, uv, vec2(1.0 / pvjAspect(), 0.0), blurR);
    blur += pvjBlur9(u_orig, uv, vec2(0.0, 1.0), blurR);
    blur *= 0.5;

    const float luma = pvjLuma(blur);
  // Threshold on blurred highlights; knee widens when threshold is lower.
    const float knee = max(0.06, mix(0.28, 0.08, thresh));
    const float w = smoothstep(thresh, min(thresh + knee, 1.0), luma);

    vec3 glow = max(blur - vec3(thresh), vec3(0.0)) * w;
    glow *= halationColor;
    const float gammaExp = mix(0.35, 2.5, gamma);
    glow = pow(max(glow, vec3(0.0)), vec3(1.0 / gammaExp));
    glow *= strength * 2.0;
    const float l = pvjLuma(glow);
    glow = mix(vec3(l), glow, 1.0 + filmSat * 1.25);

    return pvjCompositeOrig(uv, glow, blend, 1);
}

vec3 pvjGhostShape(vec2 uv, vec2 center, float size, int shape, vec3 col, float centerB, float edgeB)
{
    vec2 p = (uv - center) / max(size, 0.001);
    float d = length(p);
    float ring = smoothstep(1.0, 0.7, d) * edgeB + smoothstep(0.3, 0.0, d) * centerB;
    if (shape == 1) {
        float a = atan(p.y, p.x);
        ring *= abs(cos(a * 3.0)) * 0.6 + 0.4;
    } else if (shape == 2) {
        ring *= smoothstep(0.15, 0.0, abs(p.y));
    } else if (shape == 3) {
        ring *= smoothstep(1.0, 0.85, d);
    } else if (shape == 4) {
        ring *= exp(-d * d * 3.0);
    } else if (shape == 5) {
        float a = atan(p.y, p.x);
        ring *= abs(sin(a * 12.0)) * 0.5 + 0.5;
    }
    return col * ring;
}

vec3 pvjLensFlare(vec2 uv)
{
    vec3 src = pvjSampleRgb(u_orig, uv);
    const float blend = ubuf.params.x;
    vec2 flarePos = ubuf.params.yz;
    vec2 lensCenter = ubuf.light[0].xy;
    float gScale = ubuf.light[0].z;
    float anam = ubuf.light[0].w;
    float defocus = ubuf.light[1].x;
    float gBright = ubuf.light[1].y;
    float gSat = ubuf.light[1].z;
    float colorise = ubuf.light[1].w;
    vec3 colorizeCol = ubuf.light[2].rgb;
    int blades = int(ubuf.params3.y + 0.5);
    float apertureAngle = ubuf.params3.z * PVJ_PI / 180.0;

    vec2 p = pvjUvCentered(uv);
    vec2 fp = pvjUvCentered(flarePos);
    vec2 lc = pvjUvCentered(lensCenter);
    vec2 axis = normalize(fp - lc + vec2(1e-5));

    vec3 acc = vec3(0.0);
    float dist = length(p - fp);
    acc += vec3(1.0, 0.85, 0.5) * exp(-dist * 18.0 / max(gScale, 0.05)) * ubuf.light[3].x;
    acc += ubuf.light[3].rgb * exp(-dist * 40.0) * 0.35;

    float star = pvjStar(p - fp, blades, apertureAngle, 0.3);
    acc += ubuf.light[4].rgb * star * ubuf.light[4].w;

    for (int g = 0; g < 4; ++g) {
        vec4 gShape = ubuf.light[5 + g];
        const int shapeId = int(gShape.x + 0.5);
        if (shapeId <= 0) {
            continue;
        }
        float pos = gShape.y;
        float size = gShape.z;
        float centerB = gShape.w;
        vec4 gExtra = ubuf.light[9 + g];
        vec2 ghostCenter = lc + axis * pos * 0.35;
        acc += pvjGhostShape(uv, ghostCenter + 0.5, size * 0.08 * gScale, shapeId,
                             gExtra.rgb, centerB, gExtra.w) * gExtra.y;
    }

    vec2 q = p;
    q.x *= 1.0 + anam * 2.0;
    float glare = exp(-length(q - fp) * 4.0) * (1.0 - length(p) * 1.2);
    acc += ubuf.light[3].rgb * max(glare, 0.0) * gBright;

    acc = mix(acc, acc * colorizeCol, colorise);
    acc *= mix(1.0, gSat, 0.5);
    if (defocus > 0.01) {
        acc = pvjBlur9(u_orig, uv, vec2(1.0), defocus * 0.01) * 0.3 + acc * 0.7;
    }
    return mix(src, src + acc, blend);
}

vec3 pvjLensReflections(vec2 uv)
{
    vec3 src = pvjSampleRgb(u_orig, uv);
    const float blend = ubuf.params.x;
    const float thresh = ubuf.params.y;
    float luma = pvjLuma(src);
    if (luma < thresh) {
        return src;
    }
    vec2 p = pvjUvCentered(uv);
    float bright = ubuf.params.z;
    float gamma = max(ubuf.params.w, 0.01);
    vec3 tint = ubuf.light[0].rgb;
    float smoothK = ubuf.light[0].w;

    vec3 refl = vec3(0.0);
    float rings = 3.0 + ubuf.light[1].y * 5.0;
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float r = 0.08 + fi * 0.06;
        float ring = smoothstep(r + 0.02, r - 0.02, length(p));
        ring = pow(ring, 1.0 + smoothK * 3.0);
        float chroma = ubuf.light[1].w * fi * 0.1;
        refl += tint * ring * (1.0 - fi / rings);
        refl.r += chroma;
        refl.b -= chroma;
    }
    float eclipse = ubuf.light[1].x;
    if (abs(eclipse) > 0.01) {
        float e = smoothstep(0.0, abs(eclipse), dot(p, normalize(vec2(1.0, 0.3))));
        refl *= mix(1.0, e, abs(eclipse));
    }
    refl = pow(max(refl, vec3(0.0)), vec3(1.0 / gamma)) * bright;
    return mix(src, src + refl, blend);
}

// Volumetric light rays: radial/parallel march through u_orig (single pass, ESSL-safe loop).
vec3 pvjLightRays(vec2 uv)
{
    const float blend = ubuf.params.x;
    const float thresh = ubuf.params.y;
    const float lengthK = clamp(ubuf.params.z, 0.0, 1.0);
    const float soften = clamp(ubuf.params.w, 0.0, 1.0);
    const int sourceMode = int(ubuf.rotation.y + 0.5);
    const int dirMode = int(ubuf.rotation.z + 0.5);
    const float rayX = ubuf.light[0].x;
    const float rayY = ubuf.light[0].y;
    const float angleDeg = ubuf.light[0].z;
    const float brightness = ubuf.light[0].w;
    const float saturation = ubuf.light[1].x;
    const int ccdBloom = int(ubuf.light[1].y + 0.5);
    const int compMode = int(ubuf.light[1].z + 0.5);

    vec2 rayDir;
    if (dirMode == 0) {
        vec2 origin = vec2(rayX, rayY);
        vec2 d = uv - origin;
        rayDir = length(d) > 1e-5 ? normalize(d) : vec2(0.0, -1.0);
    } else {
        float a = angleDeg * PVJ_PI / 180.0;
        rayDir = vec2(cos(a), sin(a));
    }

    const int numSamples = 24;
    const float stepLen = mix(0.003, 0.045, lengthK) * (1.0 + soften * 0.8);
    const float knee = max(0.06, (1.0 - thresh) * 0.25);

    vec3 rays = vec3(0.0);
    float weightSum = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        float w = 1.0 - float(i) / float(numSamples) * 0.88;
        vec2 suv = clamp(uv - rayDir * float(i) * stepLen, vec2(0.001), vec2(0.999));
        vec3 s = pvjSampleRgb(u_orig, suv);
        float lum = pvjLuma(s);
        if (sourceMode == 1) {
            const vec2 px = vec2(0.003 / pvjAspect(), 0.003);
            float gx = pvjLuma(pvjSampleRgb(u_orig, suv + vec2(px.x, 0.0)))
                     - pvjLuma(pvjSampleRgb(u_orig, suv - vec2(px.x, 0.0)));
            float gy = pvjLuma(pvjSampleRgb(u_orig, suv + vec2(0.0, px.y)))
                     - pvjLuma(pvjSampleRgb(u_orig, suv - vec2(0.0, px.y)));
            lum = length(vec2(gx, gy));
        }
        float mask = smoothstep(thresh, min(thresh + knee, 1.0), lum);
        rays += s * mask * w;
        weightSum += w;
    }
    rays /= max(weightSum, 1e-4);

    if (ccdBloom == 1) {
        rays *= 1.0 + lengthK * 2.2;
    } else if (ccdBloom == 2) {
        rays *= 1.0 + lengthK * 0.65;
    }
    rays *= brightness * 2.8;
    float sat = 1.0 + saturation * 1.6;
    rays = mix(vec3(pvjLuma(rays)), rays, sat);

    return pvjCompositeOrig(uv, rays, blend, compMode);
}

void main()
{
    vec2 uv = v_uv;
    vec3 src = pvjSampleRgb(u_orig, uv);
    vec3 outRgb = src;
    const int id = pvjEffectId();

    if (id == 0) {
        outRgb = pvjApertureDiffraction(uv, src);
    } else if (id == 1) {
        outRgb = pvjHalation(uv);
    } else if (id == 2) {
        outRgb = pvjLensFlare(uv);
    } else if (id == 3) {
        outRgb = pvjLensReflections(uv);
    } else if (id == 4) {
        outRgb = pvjLightRays(uv);
    } else if (id == 5) {
        outRgb = pvjGlow(uv);
    }

    vec4 origA = pvjSampleRgba(u_orig, uv);
    fragColor = vec4(clamp(outRgb, 0.0, 1.0), origA.a);
}
