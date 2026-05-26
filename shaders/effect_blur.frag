#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
    vec4 params2;
} ubuf;

// Included by effect_*.frag after the `ubuf` uniform block is declared.

const float PVJ_PI = 3.14159265359;
const vec3 PVJ_LUMA = vec3(0.2126, 0.7152, 0.0722);

int pvjEffectId() { return int(ubuf.rotation.w + 0.5); }
float pvjTime() { return ubuf.scaleOffset.z; }
float pvjAspect() { return max(ubuf.scaleOffset.w, 0.001); }
int pvjPass() { return int(ubuf.params2.x + 0.5); }

vec2 pvjUvCentered(vec2 uv) { return uv - 0.5; }
vec2 pvjUvFromCentered(vec2 p) { return p + 0.5; }

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

vec3 pvjBlendOverlay(vec3 a, vec3 b)
{
    vec3 low = 2.0 * a * b;
    vec3 high = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
    return mix(low, high, step(0.5, a));
}

vec3 pvjBlendSoftLight(vec3 a, vec3 b)
{
    return mix(2.0 * a * b + a * a * (1.0 - 2.0 * b),
               sqrt(a) * (2.0 * b - 1.0) + 2.0 * a * (1.0 - b),
               step(0.5, b));
}

vec3 pvjBlendHardLight(vec3 a, vec3 b)
{
    return pvjBlendSoftLight(b, a);
}

vec3 pvjBlendColorDodge(vec3 a, vec3 b)
{
    return a / max(1.0 - b, 1e-4);
}

vec3 pvjBlendColorBurn(vec3 a, vec3 b)
{
    return 1.0 - (1.0 - a) / max(b, 1e-4);
}

vec3 pvjApplyBlendMode(vec3 a, vec3 b, int mode)
{
    if (mode == 0) return b;
    if (mode == 1) return a + b;
    if (mode == 2) return max(a - b, vec3(0.0));
    if (mode == 3) return a * b;
    if (mode == 4) return 1.0 - (1.0 - a) * (1.0 - b);
    if (mode == 5) return pvjBlendOverlay(a, b);
    if (mode == 6) return pvjBlendSoftLight(a, b);
    if (mode == 7) return pvjBlendHardLight(a, b);
    if (mode == 8) return pvjBlendColorDodge(a, b);
    if (mode == 9) return pvjBlendColorBurn(a, b);
    if (mode == 10) return min(a, b);
    if (mode == 11) return max(a, b);
    if (mode == 12) return abs(a - b);
    return a + b - 2.0 * a * b;
}


// Blur family: separable Gaussian/box, radial, directional/motion/zoom, temporal, sharpen, denoise.
vec3 pvjBlurSeparable1D(sampler2D tex, vec2 uv, vec2 dir, float rad)
{
    vec3 c = pvjSampleRgb(tex, uv) * 0.227027;
    c += pvjSampleRgb(tex, uv + dir * rad) * 0.316216;
    c += pvjSampleRgb(tex, uv - dir * rad) * 0.316216;
    c += pvjSampleRgb(tex, uv + dir * rad * 2.0) * 0.070270;
    c += pvjSampleRgb(tex, uv - dir * rad * 2.0) * 0.070270;
    return c;
}

void main()
{
    vec2 uv = v_uv;
    const float amount = ubuf.params.w;
    const float radiusPx = max(ubuf.params.y, 0.0);
    const float texel = max(ubuf.params2.z, 1.0 / 4096.0);
    float r = max(ubuf.params.x, radiusPx * amount * texel * 2.0);
    r = max(r, texel * 2.0);
    const float angle = ubuf.params.z;
    const int id = pvjEffectId();
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 rgb = tex.rgb;

    if (id == 9) {
        // Sharpen — unsharp mask
        vec2 px = vec2(1.0 / 1024.0, 1.0 / 1024.0);
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), r * 0.5) * 0.5
                  + pvjBlur9(u_tex, uv, vec2(0.0, 1.0), r * 0.5) * 0.5;
        float a = ubuf.params.w * 2.0;
        rgb = clamp(rgb + (rgb - blur) * a, 0.0, 1.0);
    } else if (id == 10) {
        // Denoise — edge-preserving blur
        vec3 c = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), r * 0.35);
        rgb = mix(rgb, c, ubuf.params.w * 0.85);
    } else if (id == 4) {
        // Radial blur — sample along radius from center
        vec2 c = uv - 0.5;
        int taps = 8;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < taps; ++i) {
            float t = float(i) / float(taps - 1) - 0.5;
            acc += pvjSampleRgb(u_tex, uv - c * t * r * 8.0 * amount);
        }
        rgb = acc / float(taps);
    } else if (id == 5 || id == 6) {
        // Directional / motion blur
        float ang = ubuf.params.z;
        vec2 dir = vec2(cos(ang), sin(ang)) * r * 12.0 * amount;
        rgb = pvjSampleRgb(u_tex, uv) * 0.2;
        for (int i = 1; i <= 8; ++i) {
            float t = float(i) / 8.0;
            rgb += pvjSampleRgb(u_tex, uv + dir * t) * 0.1;
        }
    } else if (id == 7) {
        // Zoom blur
        vec2 c = uv - 0.5;
        rgb = vec3(0.0);
        for (int i = 0; i < 10; ++i) {
            float t = 1.0 - float(i) / 10.0 * r * 20.0 * amount;
            rgb += pvjSampleRgb(u_tex, c * t + 0.5);
        }
        rgb /= 10.0;
    } else if (id == 8) {
        // Temporal — mix with time-offset sample (no history buffer)
        float h = pvjHash(uv + pvjTime() * 0.01);
        vec2 off = vec2(cos(h * 6.28), sin(h * 6.28)) * r * 2.0;
        rgb = mix(pvjSampleRgb(u_tex, uv), pvjSampleRgb(u_tex, uv + off), amount * 0.5);
    } else if (id == 11) {
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
        // Box / Gaussian / fast — separable 1D passes (H then V).
        // `r` is in UV space; axis is a unit vector (not texel-scaled).
        vec2 dir = ubuf.params2.y > 0.5 ? vec2(0.0, 1.0) : vec2(1.0, 0.0);
        float rad = r * ((id == 1) ? 0.75 : 1.0);
        vec3 blurred;
        if (id == 3) {
            blurred = vec3(0.0);
            for (int i = -2; i <= 2; ++i) {
                blurred += pvjSampleRgb(u_tex, uv + dir * rad * float(i));
            }
            blurred /= 5.0;
        } else {
            blurred = pvjBlurSeparable1D(u_tex, uv, dir, rad);
        }
        const bool finalPass = ubuf.params2.y > 0.5 || id == 1;
        rgb = finalPass ? mix(tex.rgb, blurred, amount) : blurred;
    }
    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
