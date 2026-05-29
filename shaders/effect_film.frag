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
const vec3 PVJ_LUMA = vec3(0.2126, 0.7152, 0.0722);

int pvjEffectId() { return int(ubuf.rotation.w + 0.5); }
float pvjTime() { return ubuf.scaleOffset.z; }
float pvjAspect() { return max(ubuf.scaleOffset.w, 0.001); }

float pvjHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float pvjNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = pvjHash(i);
    float b = pvjHash(i + vec2(1.0, 0.0));
    float c = pvjHash(i + vec2(0.0, 1.0));
    float d = pvjHash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float hue2rgb(float p, float q, float t)
{
    float x = t;
    if (x < 0.0) x += 1.0;
    if (x > 1.0) x -= 1.0;
    if (x < 1.0 / 6.0) return p + (q - p) * 6.0 * x;
    if (x < 0.5) return q;
    if (x < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - x) * 6.0;
    return p;
}

vec3 pvjHsl2rgb(vec3 hsl)
{
    float h = fract(hsl.x);
    float s = hsl.y;
    float l = hsl.z;
    if (s < 1e-5) return vec3(l);
    float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    float p = 2.0 * l - q;
    return vec3(
        hue2rgb(p, q, h + 1.0 / 3.0),
        hue2rgb(p, q, h),
        hue2rgb(p, q, h - 1.0 / 3.0));
}

vec3 pvjRgb2hsl(vec3 c)
{
    float maxc = max(max(c.r, c.g), c.b);
    float minc = min(min(c.r, c.g), c.b);
    float l = (maxc + minc) * 0.5;
    float delta = maxc - minc;
    float h = 0.0;
    float s = 0.0;
    if (delta > 1e-6) {
        s = l < 0.5 ? delta / (maxc + minc) : delta / (2.0 - maxc - minc);
        if (maxc == c.r) h = (c.g - c.b) / delta + (c.g < c.b ? 6.0 : 0.0);
        else if (maxc == c.g) h = (c.b - c.r) / delta + 2.0;
        else h = (c.r - c.g) / delta + 4.0;
        h /= 6.0;
    }
    return vec3(h, s, l);
}

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

vec3 pvjTempTint(vec3 rgb, float tempNorm, float tint)
{
    float warm = (tempNorm - 0.5) * 2.0;
    vec3 wb = vec3(1.0 + warm * 0.12, 1.0, 1.0 - warm * 0.12);
    wb.g += tint * 0.08;
    wb.r -= tint * 0.04;
    wb.b -= tint * 0.04;
    return rgb * wb;
}

vec3 pvjFilmLookPreset(vec3 rgb, int preset)
{
    vec3 hsl = pvjRgb2hsl(rgb);
    if (preset == 0) { // Default 65mm
        hsl.x += 0.02;
        hsl.y *= 1.05;
        hsl.z = hsl.z * 0.98 + 0.01;
    } else if (preset == 1) { // Default 35mm
        hsl.y *= 1.08;
        hsl.z = hsl.z * 0.97 + 0.015;
    } else if (preset == 2) { // Cinematic
        hsl.x += 0.04;
        hsl.y *= 1.12;
        hsl.z = pow(hsl.z, 0.92);
    } else if (preset == 3) { // Nostalgic
        hsl.x += 0.06;
        hsl.y *= 0.85;
        hsl.z = hsl.z * 0.9 + 0.06;
    } else if (preset == 4) { // Bleach Bypass
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(vec3(l), rgb, 0.35);
        rgb = (rgb - 0.5) * 1.25 + 0.5;
        return clamp(rgb, 0.0, 1.0);
    } else if (preset == 5) { // Rochester
        hsl.x += 0.58;
        hsl.y *= 0.95;
        hsl.z *= 0.96;
    } else if (preset == 6) { // Akasaka
        hsl.x += 0.08;
        hsl.y *= 1.1;
        hsl.z *= 1.02;
    } else if (preset == 7) { // Elated
        hsl.y *= 1.18;
        hsl.z = hsl.z * 1.04;
    } else if (preset == 8) { // Vintage
        hsl.x += 0.1;
        hsl.y *= 0.78;
        hsl.z = hsl.z * 0.88 + 0.05;
    } else if (preset == 9) { // Aurora (Resolve 21)
        hsl.y *= 1.2;
        hsl.z = pow(hsl.z, 0.88);
        rgb = pvjHsl2rgb(hsl);
        rgb = (rgb - 0.5) * 1.15 + 0.5;
        return clamp(rgb, 0.0, 1.0);
    }
    return clamp(pvjHsl2rgb(hsl), 0.0, 1.0);
}

vec3 pvjFilmColorGrade(vec3 rgb)
{
    float exposure = ubuf.params.x;
    float contrast = ubuf.params.y;
    float hiFade = ubuf.params.z;
    float fadeRolloff = ubuf.params.w;
    float tempNorm = ubuf.params2.y;
    float tint = ubuf.params2.z;
    float subSat = ubuf.params2.w;
    float saturation = ubuf.params3.x;
    float richness = ubuf.params3.y;

    rgb *= pow(2.0, exposure * 2.0);
    rgb = (rgb - 0.5) * (1.0 + contrast * 2.0) + 0.5;
    rgb = pvjTempTint(rgb, tempNorm, tint);

    float luma = dot(rgb, PVJ_LUMA);
    rgb = mix(rgb, vec3(luma), hiFade * smoothstep(0.55, 1.0, luma));
    float blackLift = fadeRolloff * 0.08;
    rgb = mix(vec3(blackLift), rgb, smoothstep(0.0, fadeRolloff * 0.25 + 0.001, luma));

    vec3 hsl = pvjRgb2hsl(rgb);
    float satMul = 1.0 + saturation * 2.0;
    hsl.y *= satMul;
  // Subtractive saturation: reduce sat in bright areas
    hsl.y *= 1.0 - subSat * smoothstep(0.4, 1.0, hsl.z);
  // Richness: boost low-sat regions
    hsl.y += richness * (1.0 - hsl.y) * 0.5;
    return clamp(pvjHsl2rgb(hsl), 0.0, 1.0);
}

vec3 pvjSplitTone(vec3 rgb, float amount, float hueRad, float balance)
{
    float luma = dot(rgb, PVJ_LUMA);
    float shadowW = 1.0 - smoothstep(balance, balance + 0.35, luma);
    float highlightW = smoothstep(balance - 0.35, balance, luma);
    vec3 warmHi = pvjHsl2rgb(vec3(fract(hueRad / (2.0 * PVJ_PI)), 0.35, 0.55));
    vec3 coolSh = pvjHsl2rgb(vec3(fract(hueRad / (2.0 * PVJ_PI) + 0.5), 0.3, 0.35));
    rgb = mix(rgb, rgb * warmHi * 2.0, highlightW * amount);
    rgb = mix(rgb, rgb * coolSh * 2.0, shadowW * amount);
    return clamp(rgb, 0.0, 1.0);
}

vec3 pvjVignette(vec3 rgb, vec2 uv, float amount, float size, float softness, float roundness)
{
    vec2 p = (uv - 0.5) * vec2(mix(pvjAspect(), 1.0, roundness), 1.0);
    float d = length(p) * (1.4 / max(size, 0.2));
    float v = 1.0 - smoothstep(0.4, 0.4 + softness + 0.3, d) * amount;
    return rgb * v;
}

vec3 pvjHalation(vec3 src, sampler2D tex, vec2 uv, float amount, float thresh, float size, float hueRad)
{
    float r = 0.004 + size * 0.07;
    vec3 blur = pvjBlur9(tex, uv, vec2(1.0, 0.0), r) + pvjBlur9(tex, uv, vec2(0.0, 1.0), r);
    blur *= 0.5;
    float luma = dot(blur, PVJ_LUMA);
    float knee = 0.12;
    float w = smoothstep(thresh, min(thresh + knee, 1.0), luma);
    vec3 tint = pvjHsl2rgb(vec3(fract(hueRad / (2.0 * PVJ_PI)), 0.6, 0.55));
    vec3 glow = max(blur - vec3(thresh), 0.0) * w * tint * amount * 2.0;
    return src + glow;
}

vec3 pvjBloom(vec3 src, sampler2D tex, vec2 uv, float amount, float thresh, float size)
{
    float r = 0.004 + size * 0.07;
    vec3 blur = pvjBlur9(tex, uv, vec2(1.0, 0.0), r) + pvjBlur9(tex, uv, vec2(0.0, 1.0), r);
    blur *= 0.5;
    float luma = dot(blur, PVJ_LUMA);
    float knee = 0.12;
    float w = smoothstep(thresh, min(thresh + knee, 1.0), luma);
    vec3 glow = max(blur - vec3(thresh), 0.0) * w;
    return src + glow * amount * 2.0;
}

vec3 pvjFilmGrain(vec3 rgb, vec2 uv, float grainSize, float strength, float mono, float t)
{
    float scale = mix(200.0, 1200.0, grainSize);
    float n = pvjNoise(uv * scale + vec2(t * 17.0, t * 13.0));
    if (mono < 0.5) {
        rgb += vec3(n - 0.5) * strength * 0.35;
    } else {
        rgb += (n - 0.5) * strength * 0.35;
    }
    return rgb;
}

float pvjGateAspect(int ratioIdx)
{
    if (ratioIdx == 0) return 4.0 / 3.0;
    if (ratioIdx == 1) return 16.0 / 10.0;
    if (ratioIdx == 2) return 16.0 / 9.0;
    if (ratioIdx == 3) return 1.85;
    if (ratioIdx == 4) return 2.39;
    if (ratioIdx == 5) return 5.0 / 3.0;  // Super 16
    if (ratioIdx == 6) return 4.0 / 3.0;  // Super 8
    return 16.0 / 9.0;
}

void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 rgb = src;
    int id = pvjEffectId();
    float blend = ubuf.params.w;
    float t = pvjTime();

    if (id == 0) { // film_look
        int preset = int(ubuf.params.x + 0.5);
        blend = ubuf.params.y;
        vec3 fx = pvjFilmLookPreset(src, preset);
        rgb = mix(src, fx, blend);
    } else if (id == 1) { // film_color
        blend = ubuf.params3.z;
        vec3 fx = pvjFilmColorGrade(src);
        rgb = mix(src, fx, blend);
    } else if (id == 2) { // film_split_tone
        float amount = ubuf.params.x;
        float hueRad = ubuf.params.y;
        float balance = ubuf.params.z;
        blend = ubuf.params.w;
        vec3 fx = pvjSplitTone(src, amount, hueRad, balance);
        rgb = mix(src, fx, blend);
    } else if (id == 3) { // film_vignette
        float amount = ubuf.params.x;
        float size = ubuf.params.y;
        float softness = ubuf.params.z;
        float roundness = ubuf.params2.y;
        blend = ubuf.params2.z;
        vec3 fx = pvjVignette(src, uv, amount, size, softness, roundness);
        rgb = mix(src, fx, blend);
    } else if (id == 4) { // film_halation
        float amount = ubuf.params.x;
        float thresh = ubuf.params.y;
        float size = ubuf.params.z;
        float hueRad = ubuf.rotation.y;
        blend = ubuf.params2.y;
        vec3 fx = pvjHalation(src, u_tex, uv, amount, thresh, size, hueRad);
        rgb = mix(src, fx, blend);
    } else if (id == 5) { // film_bloom
        float amount = ubuf.params.x;
        float thresh = ubuf.params.y;
        float size = ubuf.params.z;
        blend = ubuf.params2.y;
        vec3 fx = pvjBloom(src, u_tex, uv, amount, thresh, size);
        rgb = mix(src, fx, blend);
    } else if (id == 6) { // film_grain
        float grainSize = ubuf.params.x;
        float strength = ubuf.params.y;
        float mono = ubuf.rotation.y;
        blend = ubuf.params.z;
        vec3 fx = pvjFilmGrain(src, uv, grainSize, strength, mono, t);
        rgb = mix(src, fx, blend);
    } else if (id == 7) { // film_flicker
        float amount = ubuf.params.x;
        float speed = ubuf.params.y;
        blend = ubuf.params.z;
        float flick = 1.0 + sin(t * speed * 12.0) * amount * 0.15
                    + (pvjNoise(vec2(t * speed * 3.0, 0.0)) - 0.5) * amount * 0.1;
        vec3 fx = src * flick;
        rgb = mix(src, fx, blend);
    } else if (id == 8) { // film_gate_weave
        float amountH = ubuf.params.x;
        float amountV = ubuf.params.y;
        float speed = ubuf.params.z;
        blend = ubuf.params.w;
        vec2 off = vec2(sin(t * speed * 2.3) * amountH * 0.008,
                        cos(t * speed * 1.7) * amountV * 0.006);
        vec3 fx = pvjSampleRgb(u_tex, uv + off);
        rgb = mix(src, fx, blend);
    } else if (id == 9) { // film_gate
        int ratioIdx = int(ubuf.params.x + 0.5);
        float padding = ubuf.params.y;
        float softness = ubuf.params.z;
        blend = ubuf.params2.y;
        float targetAspect = pvjGateAspect(ratioIdx);
        float frameAspect = pvjAspect();
        vec2 p = uv - 0.5;
        float gate = 1.0;
        if (frameAspect > targetAspect) {
            float w = targetAspect / frameAspect;
            float edge = abs(p.x) - w * 0.5 + padding * 0.05;
            gate = 1.0 - smoothstep(0.0, softness * 0.08 + 0.002, edge);
        } else {
            float h = frameAspect / targetAspect;
            float edge = abs(p.y) - h * 0.5 + padding * 0.05;
            gate = 1.0 - smoothstep(0.0, softness * 0.08 + 0.002, edge);
        }
        vec3 fx = src * gate;
        rgb = mix(src, fx, blend);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
