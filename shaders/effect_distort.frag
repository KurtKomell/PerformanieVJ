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


vec2 distortUv(vec2 uv, int id)
{
    vec2 p = pvjUvCentered(uv);
    float amount = ubuf.params.x;
    float freq = max(ubuf.params.y, 0.01);
    float speed = ubuf.params.z;
    float t = pvjTime() + speed * 0.15;

    if (id == 0) { // ripple
        float d = length(p);
        p += normalize(p + 1e-5) * sin(d * freq * 10.0 - t * 3.0) * amount * 0.05;
    } else if (id == 1) { // wave
        p.x += sin(p.y * freq * 3.0 + t) * amount * 0.05;
        p.y += cos(p.x * freq * 3.0 + t) * amount * 0.05;
    } else if (id == 2) { // twirl
        float r = length(p);
        float a = atan(p.y, p.x) + amount * r * freq * 0.5;
        p = vec2(cos(a), sin(a)) * r;
    } else if (id == 3) { // bulge
        float r = length(p);
        p *= 1.0 + amount * (1.0 - r) * 0.5;
    } else if (id == 4) { // fisheye
        float r = length(p);
        float theta = atan(r, 0.5);
        p = normalize(p + 1e-5) * tan(theta * amount) * 0.5;
    } else if (id == 5) { // distortion
        float r = length(p);
        p *= 1.0 + amount * sin(r * freq * 8.0) * ubuf.params2.y;
    } else if (id == 6) { // bend
        p.y += sin(p.x * freq + t) * amount * 0.1;
    } else if (id == 7) { // warp
        p += vec2(sin(p.y * freq + t), cos(p.x * freq + t)) * amount * 0.04;
    } else if (id == 8) { // displacement
        p.x += ubuf.params.y * amount * 0.1;
        p.y += ubuf.params.z * amount * 0.1;
    } else if (id == 9) { // screen_shake
        p += vec2(pvjNoise(vec2(t * freq)), pvjNoise(vec2(t * freq + 1.0))) * amount * 0.05 - 0.025;
    } else if (id == 10) { // space_warper
        float a = atan(p.y, p.x) + sin(length(p) * freq + t) * amount;
        p = vec2(cos(a), sin(a)) * length(p);
    } else if (id == 11) { // shifty
        float ang = ubuf.params2.y;
        p += vec2(cos(ang), sin(ang)) * amount * 0.1;
    } else if (id == 12 || id == 13) { // mesh_warp / liquify
        p += vec2(pvjNoise(p * freq + t), pvjNoise(p * freq + t + 4.0)) * amount * 0.08;
    } else if (id == 14) { // spherize
        float r = length(p);
        p = normalize(p + 1e-5) * pow(r, 1.0 - amount * 0.5);
    } else if (id == 15) { // cylinder
        p.x = atan(p.x, p.y + 1e-5) / PVJ_PI;
    } else if (id == 16) { // cube — pseudo cubemap
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
    return pvjUvFromCentered(p);
}

void main()
{
    int id = pvjEffectId();
    vec4 orig = pvjSampleRgba(u_tex, v_uv);
    vec2 uv = distortUv(v_uv, id);
    vec3 rgb = pvjSampleRgba(u_tex, uv).rgb;
    if (id >= 17) {
        rgb = mix(orig.rgb, rgb, ubuf.params.x);
    }
    fragColor = vec4(rgb, orig.a);
}
