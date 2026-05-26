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


vec2 transformUv(vec2 uv, int id)
{
    vec2 p = pvjUvCentered(uv);
    float t = pvjTime();
    float amount = ubuf.params.x;
    if (id == 1) { // scale
        float s = max(ubuf.params.y, 0.01);
        p /= s;
    } else if (id == 2) { // rotate
        float a = ubuf.params.y;
        float ca = cos(a), sa = sin(a);
        p = vec2(ca * p.x - sa * p.y, sa * p.x + ca * p.y);
    } else if (id == 3 || id == 4 || id == 5) { // flip
        int mode = int(ubuf.params.y + 0.5);
        if (id == 4 || mode == 0) p.x = -p.x;
        if (id == 5 || mode == 1) p.y = -p.y;
        if (id == 3 && mode == 2) { p = -p; }
    } else if (id == 6 || id == 7) { // tile / motion_tile
        float rx = max(ubuf.params.x, 1.0);
        float ry = max(ubuf.params.y, 1.0);
        p = fract(p * vec2(rx, ry) + 0.5) - 0.5;
        if (ubuf.params.z > 0.5) p = abs(p);
    } else if (id == 8) { // smooth_transform — slight ease
        p *= 1.0 + amount * 0.1 * sin(pvjTime() + length(p));
    } else if (id == 9) { // zoom
        p /= max(ubuf.params.y, 0.01);
    } else if (id == 10) { // polar
        float r = length(p) * 2.0;
        float a = atan(p.y, p.x) / PVJ_PI;
        return vec2(a, r) * 0.5 + 0.5;
    } else if (id == 11) { // polarizer
        float a = atan(p.y, p.x) + ubuf.params.y;
        p = vec2(cos(a), sin(a)) * length(p);
    } else if (id == 12) { // pixelate
        float sz = max(ubuf.params.y, 1.0) / 512.0;
        uv = floor(uv / sz) * sz + sz * 0.5;
        return uv;
    } else if (id == 13) { // camera_shake
        float s = ubuf.params.y;
        p += vec2(pvjNoise(vec2(t * 10.0)), pvjNoise(vec2(t * 10.0 + 1.0))) * s * 0.06 - 0.03;
    } else if (id == 14) { // video_collage
        vec2 tile = floor(p * mix(2.0, 6.0, ubuf.params.z) + 0.5);
        p = fract(p * mix(2.0, 4.0, ubuf.params.y)) - 0.5;
        p += sin(tile + t) * ubuf.params.y * 0.02;
    } else if (id == 0) { // transform
        float s = max(ubuf.params.x, 0.01);
        float rot = ubuf.params.y;
        float ca = cos(rot), sa = sin(rot);
        p /= s;
        p = vec2(ca * p.x - sa * p.y, sa * p.x + ca * p.y);
        p += vec2(ubuf.params.z, ubuf.params.w);
    }
    return pvjUvFromCentered(p);
}

void main()
{
    int id = pvjEffectId();
    vec4 orig = pvjSampleRgba(u_tex, v_uv);
    vec2 uv = transformUv(v_uv, id);
    vec3 rgb = pvjSampleRgba(u_tex, uv).rgb;
    if (id >= 13) {
        rgb = mix(orig.rgb, rgb, ubuf.params.x);
    }
    fragColor = vec4(rgb, orig.a);
}
