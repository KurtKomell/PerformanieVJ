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


void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 rgb = tex.rgb;
    int id = pvjEffectId();
    float amount = ubuf.params.x;
    float t = pvjTime();

    if (id == 0 || id == 1) { // glow / bloom
        float thresh = ubuf.params.y;
        vec3 bright = max(rgb - thresh, 0.0);
        vec2 px = vec2(1.0 / 512.0);
        vec3 blur = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), amount * 0.02)
                  + pvjBlur9(u_tex, uv, vec2(0.0, 1.0), amount * 0.02);
        blur *= 0.5;
        if (pvjPass() == 1) {
            rgb = mix(rgb, blur, amount);
            fragColor = vec4(rgb, tex.a);
            return;
        }
        rgb += bright * amount + blur * amount * 0.5;
    } else if (id == 2) { // god rays
        vec2 c = uv - 0.5;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 12; ++i) {
            acc += pvjSampleRgb(u_tex, uv - c * float(i) * 0.02 * amount);
        }
        rgb = mix(rgb, acc / 12.0, amount);
    } else if (id == 3) { // strobe
        float phase = fract(t * ubuf.params.y);
        rgb *= step(ubuf.params.z, phase);
    } else if (id == 4) { // trails
        vec2 off = vec2(sin(t), cos(t)) * ubuf.params.y * 0.01;
        rgb = mix(pvjSampleRgb(u_tex, uv + off), rgb, ubuf.params.x);
    } else if (id == 5) { // light_leak
        float ang = ubuf.params.y;
        float leak = smoothstep(0.0, 1.0, dot(normalize(vec2(cos(ang), sin(ang))), pvjUvCentered(uv)) + 0.5);
        rgb += vec3(1.0, 0.6, 0.2) * leak * amount;
    } else if (id == 6 || id == 7 || id == 8) { // noise variants
        float n = pvjNoise(uv * 400.0 + t * ubuf.params.y);
        if (id == 7) rgb += vec3(n, pvjNoise(uv * 400.0 + 1.0), pvjNoise(uv * 400.0 + 2.0)) * amount * 0.3;
        else if (id == 8) rgb = mix(rgb, vec3(n), amount * 0.5);
        else rgb += (n - 0.5) * amount * 0.4;
    } else if (id == 9) { // halftone
        float sz = max(ubuf.params.y, 2.0) / 256.0;
        vec2 g = fract(uv / sz) - 0.5;
        float d = length(g);
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(rgb, vec3(1.0), step(d, l * amount));
    } else if (id == 10) { // vignette
        float d = length(pvjUvCentered(uv)) * 1.4;
        rgb *= 1.0 - smoothstep(0.4, 1.0, d) * amount;
    } else if (id == 11) { // spotlight
        float d = length(pvjUvCentered(uv));
        rgb *= 1.0 - smoothstep(ubuf.params.y, ubuf.params.y + ubuf.params.z, d) * amount;
    } else if (id == 12) { // drop_shadow
        float ang = ubuf.params.z;
        vec2 off = vec2(cos(ang), sin(ang)) * ubuf.params.y * 0.002;
        vec3 sh = pvjSampleRgb(u_tex, uv + off);
        rgb = mix(sh * 0.2, rgb, 1.0 - ubuf.params.x * (1.0 - dot(sh, PVJ_LUMA)));
    } else if (id == 13) { // rainbow
        float h = fract(uv.x + t * ubuf.params.y * 0.1);
        rgb = mix(rgb, pvjHsl2rgb(vec3(h, 1.0, 0.5)), amount);
    } else if (id == 14) { // prismatic
        int n = int(clamp(ubuf.params.y, 1.0, 16.0));
        vec3 acc = vec3(0.0);
        for (int i = 0; i < n; ++i) {
            float off = float(i - n / 2) * amount * 0.002;
            acc += pvjSampleRgb(u_tex, uv + vec2(off, 0.0));
        }
        rgb = acc / float(n);
    } else if (id == 15) { // replicate
        vec2 p = fract(pvjUvCentered(uv) * ubuf.params.x + 0.5) - 0.5;
        rgb = pvjSampleRgb(u_tex, pvjUvFromCentered(p * (1.0 + ubuf.params.y)));
    } else if (id == 16) { // echo
        vec2 off = vec2(ubuf.params.y * 0.02, 0.0);
        rgb = mix(pvjSampleRgb(u_tex, uv - off), rgb, 1.0 - amount);
    } else if (id == 17) { // slit_scanner
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
            vec2 c = pvjUvCentered(uv) * 2.5;
            vec2 z = vec2(0.0);
            float iter = 0.0;
            for (int i = 0; i < 12; ++i) {
                z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
                iter += step(4.0, dot(z, z));
            }
            fx = mix(rgb, vec3(1.0 - iter / 12.0), s);
        } else if (id == 24) {
            float g = smoothstep(0.2, 0.8, 1.0 - uv.y);
            fx = mix(rgb, mix(vec3(0.4, 0.6, 1.0), vec3(0.9, 0.95, 1.0), g), s);
        }
        rgb = mix(tex.rgb, fx, blend);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
