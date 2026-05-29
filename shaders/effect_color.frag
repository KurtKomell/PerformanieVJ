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
    if (id == 13) return rgb; // invert handled in main()
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

    if (id == 13) {
        float ir = ubuf.params.x;
        float ig = ubuf.params.y;
        float ib = ubuf.params.z;
        float ia = ubuf.params.w;
        vec3 rgb = tex.rgb;
        rgb.r = mix(rgb.r, 1.0 - rgb.r, ir);
        rgb.g = mix(rgb.g, 1.0 - rgb.g, ig);
        rgb.b = mix(rgb.b, 1.0 - rgb.b, ib);
        float a = mix(tex.a, 1.0 - tex.a, ia);
        fragColor = vec4(clamp(rgb, 0.0, 1.0), clamp(a, 0.0, 1.0));
        return;
    }

    if (id == 26 || id == 27 || id == 29 || id == 30 || id == 31) {
        float blend = ubuf.params.x;
        vec3 fx = tex.rgb;

        if (id == 26) {
            float srcK = mix(1000.0, 20000.0, ubuf.params.y);
            float srcTint = ubuf.params.z;
            float tgtK = mix(1000.0, 20000.0, ubuf.params.w);
            float tgtTint = ubuf.rotation.y;
            float method = ubuf.rotation.z;
            float deltaK = (tgtK - srcK) / 19000.0;
            float deltaTint = tgtTint - srcTint;
            float tempGain = 0.5;
            float tintGain = 0.4;
            if (method < 0.5) {
                tempGain = 0.5;
                tintGain = 0.4;
            } else if (method < 1.5) {
                tempGain = 0.58;
                tintGain = 0.32;
            } else if (method < 2.5) {
                tempGain = 0.42;
                tintGain = 0.28;
            } else if (method < 3.5) {
                tempGain = 0.62;
                tintGain = 0.52;
            } else {
                tempGain = 0.54;
                tintGain = 0.38;
            }
            fx.r += deltaK * tempGain;
            fx.b -= deltaK * tempGain;
            fx.g += deltaTint * tintGain;
            fx.r -= deltaTint * 0.14 * tintGain;
            fx.b -= deltaTint * 0.14 * tintGain;
            fx = clamp(fx, 0.0, 1.0);
        } else if (id == 27) {
            float targetHue = ubuf.params.y;
            float ch = ubuf.params.z;
            float cs = ubuf.params.w;
            float cl = ubuf.params2.w;
            vec3 hsl = pvjRgb2hsl(tex.rgb);
            float dh = targetHue - hsl.x * 6.2831853;
            dh = dh - 6.2831853 * floor(dh / 6.2831853 + 0.5);
            hsl.x = fract(hsl.x + (dh / 6.2831853) * ch);
            hsl.y = mix(hsl.y, 0.5, cs * 0.35);
            fx = pvjHsl2rgb(hsl);
            float luma = dot(fx, PVJ_LUMA);
            float lTarget = mix(luma, 0.5, cl);
            fx = mix(fx, fx * (lTarget / max(luma, 1e-4)), cl);
            fx = clamp(fx, 0.0, 1.0);
        } else if (id == 29) {
            float strength = ubuf.params.y;
            float matchMode = ubuf.params.z;
            vec3 ref = vec3(0.5);
            if (matchMode < 0.5) {
                float l = dot(tex.rgb, PVJ_LUMA);
                fx = mix(tex.rgb, vec3(l), strength * 0.45);
            } else if (matchMode < 1.5) {
                fx = mix(ref, tex.rgb, 1.0 - strength * 0.35);
            } else {
                fx = mix(ref, tex.rgb, 1.0 - strength * 0.5);
                float l = dot(fx, PVJ_LUMA);
                fx = mix(vec3(l), fx, 0.9);
            }
        } else if (id == 30) {
            float amount = ubuf.params.y;
            float size = ubuf.params.z;
            float lo = ubuf.params.w;
            float hi = ubuf.rotation.y;
            float soft = max(ubuf.rotation.z, 0.02);
            float luma = dot(tex.rgb, PVJ_LUMA);
            vec3 blurred = pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.003 + size * 0.03);
            float blurL = dot(blurred, PVJ_LUMA);
            float detail = luma - blurL;
            float mask = smoothstep(lo, lo + soft * 0.25, luma)
                       * (1.0 - smoothstep(hi - soft * 0.25, hi, luma));
            float l2 = clamp(luma + detail * amount * mask * 2.5, 0.0, 1.0);
            fx = clamp(tex.rgb + vec3(l2 - luma), 0.0, 1.0);
        } else if (id == 31) {
            float strength = ubuf.params.y;
            float hazeHue = ubuf.params.z;
            vec3 hazeRgb = pvjHsl2rgb(vec3(fract(hazeHue / 6.2831853), 0.55, 0.5));
            vec3 comp = clamp(1.0 - hazeRgb, 0.0, 1.0);
            fx = mix(tex.rgb, pvjBlur9(u_tex, uv, vec2(1.0, 0.0), 0.008 + abs(strength) * 0.025),
                     abs(strength) * 0.45);
            fx = (fx - 0.5) * (1.0 + strength * 0.55) + 0.5;
            fx = mix(fx, fx * comp, strength * 0.2);
            float l = dot(fx, PVJ_LUMA);
            fx = mix(vec3(l), fx, 1.0 + strength * 0.25);
            fx = clamp(fx, 0.0, 1.0);
        }

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
