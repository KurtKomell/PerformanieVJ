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


const int MASK_NONE = 0;
const int MASK_RECT = 1;
const int MASK_CIRCLE = 2;
const int MASK_SOFT = 3;
const int MASK_ELLIPSE = 4;
const int MASK_CUSTOM = 5;

void main()
{
    vec4 tex = texture(u_tex, v_uv);
    vec2 uv = v_uv;
    int id = pvjEffectId();

    if (id == 0) { // linear_mask
        float ang = ubuf.params.x;
        float soft = ubuf.params.y;
        vec2 p = pvjUvCentered(uv);
        float d = dot(p, vec2(cos(ang), sin(ang)));
        float m = smoothstep(-soft, soft, d);
        fragColor = vec4(tex.rgb, tex.a * m);
        return;
    }

    if (id == 2) { // crop
        vec2 lo = vec2(ubuf.params.x, ubuf.params.y);
        vec2 hi = vec2(ubuf.params.z, ubuf.params.w);
        if (uv.x < lo.x || uv.y < lo.y || uv.x > hi.x || uv.y > hi.y) {
            fragColor = vec4(0.0);
        } else {
            fragColor = tex;
        }
        return;
    }

    if (id == 3) { // crop_rectangle
        vec2 lo = vec2(ubuf.params.x, ubuf.params.y);
        vec2 sz = vec2(max(ubuf.params.z, 0.001), max(ubuf.params.w, 0.001));
        vec2 hi = lo + sz;
        if (uv.x < lo.x || uv.y < lo.y || uv.x > hi.x || uv.y > hi.y) {
            fragColor = vec4(0.0);
        } else {
            fragColor = tex;
        }
        return;
    }

    // mask (id == 1)
    vec2 center = vec2(0.5, 0.5);
    int mt = int(ubuf.params.x + 0.5);
    float sizeX = max(ubuf.params.y, 1e-4);
    float sizeY = max(ubuf.params.z, 1e-4);
    float smoothness = clamp(ubuf.params.w, 0.001, 0.5);
    vec2 p = uv - center;
    float m = 1.0;

    if (mt == MASK_NONE) {
        m = 1.0;
    } else if (mt == MASK_RECT) {
        vec2 h = abs(p) * 2.0;
        float dx = max(h.x / sizeX, h.y / sizeY);
        m = 1.0 - smoothstep(1.0 - smoothness, 1.0 + smoothness, dx);
    } else if (mt == MASK_CIRCLE) {
        float r = min(sizeX, sizeY);
        float d = length(p) * 2.0 / r;
        m = 1.0 - smoothstep(1.0 - smoothness, 1.0 + smoothness, d);
    } else if (mt == MASK_SOFT) {
        vec2 h = abs(p) * 2.0;
        float dx = max(h.x / sizeX, h.y / sizeY);
        m = smoothstep(1.0 + smoothness, 1.0 - smoothness, dx);
    } else if (mt == MASK_ELLIPSE) {
        float d = length(vec2((p.x * 2.0) / sizeX, (p.y * 2.0) / sizeY));
        m = 1.0 - smoothstep(1.0 - smoothness, 1.0 + smoothness, d);
    } else if (mt == MASK_CUSTOM) {
        float a = atan(p.y, p.x);
        float wave = 0.5 + 0.5 * cos(a * 6.0);
        float r = min(sizeX, sizeY);
        float d = (length(p) * 2.0 / r) * mix(0.85, 1.15, wave);
        m = 1.0 - smoothstep(1.0 - smoothness, 1.0 + smoothness, d);
    }

    fragColor = vec4(tex.rgb, tex.a * clamp(m, 0.0, 1.0));
}
