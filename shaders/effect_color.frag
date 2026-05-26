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


vec3 applyColorEffect(vec3 rgb, int id)
{
    float amount = ubuf.params.x;
    float b = ubuf.params.y;
    float c = ubuf.params.z;
    float s = ubuf.params.w;

    if (id == 4) return rgb + vec3(b); // brightness
    if (id == 5) return (rgb - 0.5) * max(c, 0.001) + 0.5; // contrast
    if (id == 6) return pow(max(rgb, vec3(0.0)), vec3(1.0 / max(b, 0.05))); // gamma uses params.y as value
    if (id == 7) return rgb * pow(2.0, b); // exposure
    if (id == 8) { float l = dot(rgb, PVJ_LUMA); return mix(vec3(l), rgb, s); } // saturation
    if (id == 9 || id == 21) { vec3 hsl = pvjRgb2hsl(rgb); hsl.x = fract(hsl.x + b / 6.2831853); return pvjHsl2rgb(hsl); }
    if (id == 13) return 1.0 - rgb; // invert
    if (id == 14) return mix(rgb, 1.0 - rgb, step(b, dot(rgb, PVJ_LUMA))); // solarize
    if (id == 15) { vec3 hsl = pvjRgb2hsl(rgb); hsl.x = fract(b / 6.2831853); return mix(rgb, pvjHsl2rgb(hsl), s); } // tint
    if (id == 16) { float l = dot(rgb, PVJ_LUMA); return mix(rgb, vec3(l), s); } // black_white
    if (id == 17) { float lv = max(b, 2.0); return floor(rgb * lv) / lv; } // posterize
    if (id == 18) return step(b, dot(rgb, PVJ_LUMA)).xxx; // threshold
    if (id == 19) { vec3 hsl = pvjRgb2hsl(rgb); hsl.x = fract(b / 6.2831853); hsl.y = s; return pvjHsl2rgb(hsl); } // colorize
    if (id == 20) return rgb; // chromatic handled in main
    if (id == 3) return rgb * vec3(b, c, s); // color_balance rgb gains
    if (id == 2) return rgb * b; // intensity
    if (id == 11) { // curves — lift shadows / gain highlights
        return pow(clamp(rgb, 0.0, 1.0), vec3(1.0 / max(0.2 + c * 0.8, 0.1)));
    }
    if (id == 12) { // levels
        float lo = b * 0.5;
        float hi = 0.5 + c * 0.5;
        return clamp((rgb - lo) / max(hi - lo, 0.01), 0.0, 1.0);
    }
    if (id == 22) { // selective color — boost saturation in hue band
        vec3 hsl = pvjRgb2hsl(rgb);
        float dh = abs(hsl.x - fract(b));
        dh = min(dh, 1.0 - dh);
        if (dh < 0.1) hsl.y *= 1.0 + s;
        return pvjHsl2rgb(hsl);
    }
    if (id == 23) { // channel mixer — simplified
        return vec3(dot(rgb, vec3(b, c, s)), dot(rgb, vec3(c, s, b)), dot(rgb, vec3(s, b, c)));
    }
    if (id == 24) return mix(rgb, rgb.bgr, s); // color_lookup approx
    if (id == 0 || id == 1 || id == 10) {
        rgb += b;
        rgb = (rgb - 0.5) * max(c, 0.001) + 0.5;
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(vec3(l), rgb, s);
        vec3 hsl = pvjRgb2hsl(clamp(rgb, 0.0, 1.0));
        hsl.x = fract(hsl.x + ubuf.params2.y / 6.2831853);
        return pvjHsl2rgb(hsl);
    }
    return rgb;
}

void main()
{
    vec2 uv = v_uv;
    int id = pvjEffectId();
    vec4 tex = pvjSampleRgba(u_tex, uv);

    if (id >= 25) {
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

    if (id == 20) {
        float dist = ubuf.params.x * 0.01;
        float ang = ubuf.params.y;
        vec2 off = vec2(cos(ang), sin(ang)) * dist;
        vec3 col;
        col.r = texture(u_tex, clamp(uv + off, 0.001, 0.999)).r;
        col.g = tex.g;
        col.b = texture(u_tex, clamp(uv - off, 0.001, 0.999)).b;
        fragColor = vec4(col, tex.a);
        return;
    }

    vec3 rgb = applyColorEffect(tex.rgb, id);
    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
