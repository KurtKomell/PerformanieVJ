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


vec3 sobel(sampler2D tex, vec2 uv, float scale)
{
    vec2 px = vec2(scale / 512.0);
    vec3 tl = pvjSampleRgb(tex, uv + vec2(-px.x, -px.y));
    vec3 tm = pvjSampleRgb(tex, uv + vec2(0.0, -px.y));
    vec3 tr = pvjSampleRgb(tex, uv + vec2(px.x, -px.y));
    vec3 ml = pvjSampleRgb(tex, uv + vec2(-px.x, 0.0));
    vec3 mr = pvjSampleRgb(tex, uv + vec2(px.x, 0.0));
    vec3 bl = pvjSampleRgb(tex, uv + vec2(-px.x, px.y));
    vec3 bm = pvjSampleRgb(tex, uv + vec2(0.0, px.y));
    vec3 br = pvjSampleRgb(tex, uv + vec2(px.x, px.y));
    vec3 gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    vec3 gy = -tl - 2.0 * tm - tr + bl + 2.0 * bm + br;
    return sqrt(gx * gx + gy * gy);
}

void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 rgb = tex.rgb;
    int id = pvjEffectId();
    float amount = ubuf.params.x;
    float detail = ubuf.params.y;
    float t = pvjTime();

    if (id == 0 || id == 2) { // edges / find_edges
        vec3 e = sobel(u_tex, uv, detail + 1.0);
        rgb = mix(rgb, e, amount);
    } else if (id == 1) { // emboss
        vec2 px = vec2(1.0 / 512.0);
        vec3 off = pvjSampleRgb(u_tex, uv + px) - pvjSampleRgb(u_tex, uv - px);
        rgb = clamp(rgb + off * amount, 0.0, 1.0);
    } else if (id == 3) { // glow_edges
        vec3 e = sobel(u_tex, uv, detail + 1.0);
        rgb += e * amount * 2.0;
    } else if (id == 4) { // crt
        float scan = sin(uv.y * 800.0) * 0.04 * amount;
        rgb -= scan;
        rgb.r *= 1.0 + 0.05 * amount;
        rgb.b *= 1.0 - 0.05 * amount;
    } else if (id == 5 || id == 6) { // vhs / vhsifyer
        float n = pvjNoise(vec2(uv.y * 100.0, t));
        rgb = mix(rgb, pvjSampleRgb(u_tex, uv + vec2(n * 0.01 * amount, 0.0)), 0.7);
        rgb *= 1.0 - step(0.98, fract(uv.y * 200.0 + t)) * amount * 0.5;
    } else if (id == 7) { // film_grain
        rgb += (pvjNoise(uv * 800.0 + t) - 0.5) * amount * 0.3;
    } else if (id == 8) { // scanlines
        rgb *= 1.0 - step(0.5, fract(uv.y * 400.0)) * amount * 0.4;
    } else if (id == 9) { // broadcast
        rgb = floor(rgb * 8.0) / 8.0;
    } else if (id == 10) { // reducto
        rgb = floor(rgb * mix(2.0, 32.0, 1.0 - amount)) / mix(2.0, 32.0, 1.0 - amount);
    } else if (id == 11) { // TVA
        rgb = vec3(pvjNoise(uv * 50.0 + t), pvjNoise(uv * 70.0), pvjNoise(uv * 90.0 + t * 2.0));
    } else if (id == 12) { // cartoon
        vec3 e = sobel(u_tex, uv, 2.0);
        rgb = floor(rgb * 6.0) / 6.0;
        rgb = mix(rgb, vec3(0.0), step(0.3, length(e)) * amount);
    } else if (id == 13) { // watercolor
        rgb = pvjBlur9(u_tex, uv, vec2(1.0), 0.004 * amount);
    } else if (id == 14) { // oil_paint
        rgb = pvjBlur9(u_tex, uv, vec2(1.0), 0.008 * amount);
        rgb = floor(rgb * 8.0) / 8.0;
    } else if (id == 15) { // night_vision
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(rgb, vec3(l * 0.2, l, l * 0.2), amount);
    } else if (id == 16) { // thermal
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(rgb, pvjHsl2rgb(vec3(0.05 - l * 0.08, 1.0, l * 0.5 + 0.2)), amount);
    } else if (id == 17) { // x_ray
        float l = dot(rgb, PVJ_LUMA);
        rgb = mix(rgb, vec3(l), amount);
        rgb = 1.0 - rgb;
    } else if (id == 18 || id == 19 || id == 20) { // duotone / tritone / gradient_map
        float l = dot(rgb, PVJ_LUMA);
        vec3 c1 = pvjHsl2rgb(vec3(0.55, 0.8, 0.3));
        vec3 c2 = pvjHsl2rgb(vec3(0.08, 0.7, 0.6));
        rgb = mix(c1, c2, l);
        if (id == 19) rgb = mix(rgb, pvjHsl2rgb(vec3(0.33, 0.6, 0.5)), 0.33);
    } else if (id == 21) { // stroke
        vec3 e = sobel(u_tex, uv, 2.0);
        rgb = mix(rgb, vec3(0.0), step(0.2, length(e)) * amount);
    } else if (id == 22 || id == 23) { // erode / dilate
        vec3 mn = rgb, mx = rgb;
        vec2 px = vec2(1.0 / 512.0);
        for (int i = -1; i <= 1; ++i)
            for (int j = -1; j <= 1; ++j) {
                vec3 s = pvjSampleRgb(u_tex, uv + vec2(float(i), float(j)) * px);
                mn = min(mn, s);
                mx = max(mx, s);
            }
        rgb = mix(rgb, id == 22 ? mn : mx, amount);
    } else if (id >= 24) {
        float blend = ubuf.params.x;
        float s = ubuf.params.y;
        float d = ubuf.params.z;
        float sz = ubuf.params.w;
        vec3 fx = rgb;
        if (id == 24) fx = floor(rgb * mix(4.0, 16.0, d)) / mix(4.0, 16.0, d);
        else if (id == 25) {
            float m = max(abs(pvjUvCentered(uv).x), abs(pvjUvCentered(uv).y));
            fx = mix(rgb, vec3(0.0), step(1.0 - sz * 0.2, m) * s);
        } else if (id == 26) {
            vec3 e = sobel(u_tex, uv, 2.0);
            fx = mix(rgb, vec3(dot(e, PVJ_LUMA)), s);
        } else if (id == 27) {
            vec3 acc = vec3(0.0);
            for (int i = -2; i <= 2; ++i) acc += pvjSampleRgb(u_tex, uv + vec2(float(i) * s * 0.003, 0.0));
            fx = acc / 5.0;
        } else if (id == 28) fx = floor(rgb * mix(6.0, 24.0, s)) / mix(6.0, 24.0, s);
        else if (id == 29) {
            rgb += (pvjNoise(uv * 300.0 + t) - 0.5) * s * 0.4;
            fx = rgb;
        } else if (id == 30) {
            float n = step(0.97, pvjNoise(vec2(floor(uv.y * 200.0), t)));
            fx = mix(rgb, rgb * 0.5, n * s);
        } else if (id == 31) {
            fx = floor(rgb * mix(4.0, 16.0, s)) / mix(4.0, 16.0, s);
        } else if (id == 32) fx = mix(rgb, rgb * (0.9 + pvjNoise(uv * 100.0) * 0.2), s);
        else if (id == 33) {
            float r = length(pvjUvCentered(uv));
            fx = mix(rgb, rgb, 1.0);
            fx *= 1.0 - smoothstep(0.35, 0.45, r) * s;
            fx = mix(fx, fx * 0.3, smoothstep(0.35, 0.45, r) * s);
        } else if (id == 34) {
            fx = rgb * 0.7;
            fx += vec3(0.0, 0.05, 0.0) * s;
            fx.r *= 0.9;
        } else if (id == 35) {
            float b = smoothstep(0.4, 0.6, length(pvjUvCentered(uv)));
            fx = mix(rgb, rgb * 0.2, b * s);
        } else if (id == 36) {
            float g = step(0.98, fract(sin(floor(uv.y * 400.0) + t * 20.0) * 43758.5453));
            fx = mix(rgb, vec3(pvjNoise(uv + t)), g * s);
        } else if (id == 37) {
            fx = mix(rgb, vec3(0.2, 0.8, 0.3), 0.15 * s);
            fx += vec3(0.0, 0.1, 0.0) * d;
        } else if (id == 38) {
            float v = length(pvjUvCentered(uv));
            fx = mix(rgb, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.003), smoothstep(0.2, 0.5, v) * s);
        } else if (id == 39) {
            vec2 p = pvjUvCentered(uv);
            p.x *= 1.0 + s * 0.3 * sign(p.x);
            fx = pvjSampleRgb(u_tex, pvjUvFromCentered(p));
        } else if (id == 40) {
            fx = rgb;
            fx = floor(fx * 8.0) / 8.0;
            fx *= 0.9;
        } else if (id == 41) {
            float r = length(pvjUvCentered(uv));
            fx = mix(rgb, rgb * 0.5, smoothstep(0.3, 0.5, r) * s);
            fx += vec3(0.05) * d;
        }
        rgb = mix(tex.rgb, fx, blend);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
