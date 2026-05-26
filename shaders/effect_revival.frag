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

float pvjResolveBlend() { return ubuf.params.x; }
float pvjResolveStrength() { return ubuf.params.y; }
float pvjResolveDetail() { return ubuf.params.z; }
float pvjResolveSize() { return ubuf.params.w; }
vec3 pvjResolveMix(vec3 src, vec3 fx) { return mix(src, fx, pvjResolveBlend()); }

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
