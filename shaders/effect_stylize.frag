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
    vec4 light[16];
} ubuf;

const float PVJ_PI = 3.14159265359;
const vec3 PVJ_LUMA = vec3(0.2126, 0.7152, 0.0722);

int pvjEffectId() { return int(ubuf.rotation.w + 0.5); }
float pvjTime() { return ubuf.scaleOffset.z; }
float pvjAspect() { return max(ubuf.scaleOffset.w, 0.001); }
float pvjPx() { return max(ubuf.rotation.y, 0.001); }

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

vec3 pvjSobel(sampler2D tex, vec2 uv, float scale)
{
    vec2 px = vec2(scale * pvjPx());
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

vec2 pvjMirrorUv(vec2 uv, vec2 center, float angle, bool flip)
{
    vec2 p = uv - center;
    p.x *= pvjAspect();
    float c = cos(angle);
    float s = sin(angle);
    vec2 r = vec2(c * p.x + s * p.y, -s * p.x + c * p.y);
    if (r.x < 0.0) {
        r.x = -r.x;
        if (flip) r.y = -r.y;
    }
    p = vec2(c * r.x - s * r.y, s * r.x + c * r.y);
    p.x /= pvjAspect();
    return clamp(p + center, vec2(0.001), vec2(0.999));
}

vec2 pvjRosetteUv(vec2 uv, vec2 center, float angle, float wedgeWidth)
{
    vec2 p = uv - center;
    p.x *= pvjAspect();
    float a = atan(p.y, p.x) + angle;
    float wedges = mix(4.0, 16.0, wedgeWidth);
    a = mod(a, PVJ_PI * 2.0 / wedges);
    if (a > PVJ_PI / wedges) a = (PVJ_PI * 2.0 / wedges) - a;
    float len = length(p);
    p = vec2(cos(a), sin(a)) * len;
    p.x /= pvjAspect();
    return clamp(p + center, vec2(0.001), vec2(0.999));
}

vec2 pvjKaleidoUv(vec2 uv, vec2 center, float centerSize, float angle, float sides)
{
    vec2 p = uv - center;
    p.x *= pvjAspect();
    float len = length(p);
    // Center Size = radius of unmodified center (0 = full kaleido, 1 = no fold)
    float maxR = length(vec2(0.5 * pvjAspect(), 0.5));
    float innerR = clamp(centerSize, 0.0, 1.0) * maxR;
    if (len < innerR) {
        return uv;
    }
    float a = atan(p.y, p.x) + angle;
    float seg = PVJ_PI * 2.0 / max(sides, 3.0);
    a = mod(a, seg);
    if (a > seg * 0.5) {
        a = seg - a;
    }
    p = vec2(cos(a), sin(a)) * len;
    p.x /= pvjAspect();
    return clamp(p + center, vec2(0.001), vec2(0.999));
}

int pvjIrisSides(int irisShape, float irisBlades)
{
    if (irisShape == 1) return 3;
    if (irisShape == 2) return 4;
    if (irisShape == 3) return 5;
    if (irisShape == 4) return 6;
    if (irisShape == 5) return 7;
    if (irisShape == 6) return 8;
    return int(clamp(round(irisBlades), 3.0, 12.0));
}

float pvjIrisNormRadius(vec2 p, int sides)
{
    if (sides <= 2) {
        return length(p);
    }
    float ang = atan(p.y, p.x);
    float rad = length(p);
    float sector = PVJ_PI * 2.0 / float(sides);
    ang = mod(ang + sector * 0.5, sector) - sector * 0.5;
    return rad / max(cos(ang), 0.001);
}

vec3 pvjLensIrisBlur(sampler2D tex, vec2 uv, float r, int sides)
{
    vec3 sum = vec3(0.0);
    float wsum = 0.0;
    for (int iy = 0; iy < 7; ++iy) {
        for (int ix = 0; ix < 7; ++ix) {
            vec2 cell = vec2(float(ix), float(iy)) / 6.0 * 2.0 - 1.0;
            vec2 ap = vec2(cell.x / pvjAspect(), cell.y);
            if (pvjIrisNormRadius(ap, sides) > 1.02) {
                continue;
            }
            vec2 off = ap * r;
            sum += pvjSampleRgb(tex, uv + off);
            wsum += 1.0;
        }
    }
    if (wsum < 1.0) {
        return pvjSampleRgb(tex, uv);
    }
    return sum / wsum;
}

vec3 pvjStylePalette(int style, vec3 src)
{
    float l = dot(src, PVJ_LUMA);
    if (style == 0) return mix(vec3(0.1, 0.05, 0.2), vec3(0.9, 0.85, 0.7), l); // Antimonocromatismo
    if (style == 1) return mix(vec3(0.05, 0.15, 0.25), vec3(0.85, 0.75, 0.55), l); // Asheville
    if (style == 2) return mix(vec3(0.15, 0.1, 0.05), vec3(0.95, 0.9, 0.8), l); // Brush Stroke
    if (style == 3) return mix(vec3(0.8, 0.1, 0.3), vec3(1.0, 0.9, 0.4), l); // Candy
    if (style == 4) return mix(vec3(0.05, 0.08, 0.12), vec3(0.75, 0.7, 0.65), l); // Chinese Brush
    if (style == 5) return mix(vec3(0.2, 0.1, 0.4), vec3(0.95, 0.7, 0.2), l); // Kandinsky
    if (style == 6) return mix(vec3(0.15, 0.05, 0.2), vec3(0.9, 0.5, 0.3), l); // Dance
    if (style == 7) return mix(vec3(0.08, 0.12, 0.18), vec3(0.85, 0.82, 0.75), l); // Edtaonisl
    if (style == 8) return mix(vec3(0.2, 0.15, 0.25), vec3(0.95, 0.92, 0.88), l); // Feather
    if (style == 9) return mix(vec3(0.1, 0.08, 0.06), vec3(0.9, 0.75, 0.6), l); // Illustrated Portrait
    if (style == 10) return mix(vec3(0.12, 0.08, 0.15), vec3(0.85, 0.65, 0.45), l); // La Muse
    if (style == 11) { // Mondrian
        vec3 c = vec3(0.95);
        if (l < 0.33) c = vec3(0.95, 0.2, 0.15);
        else if (l < 0.66) c = vec3(0.15, 0.35, 0.85);
        else c = vec3(0.95, 0.85, 0.15);
        return c;
    }
    if (style == 12) return mix(vec3(0.05, 0.05, 0.15), vec3(0.9, 0.85, 0.95), l); // Stained Glass
    if (style == 13) return mix(vec3(0.05, 0.08, 0.2), vec3(0.95, 0.75, 0.35), l); // Cafe Terrace
    if (style == 14) return mix(vec3(0.15, 0.2, 0.35), vec3(0.75, 0.8, 0.65), l); // Impressionist
    if (style == 15) return mix(vec3(0.15, 0.25, 0.55), vec3(0.95, 0.75, 0.35), l); // Scream
    if (style == 16) return mix(vec3(0.08, 0.12, 0.2), vec3(0.75, 0.55, 0.35), l); // Protocubist
    if (style == 17) return mix(vec3(0.2, 0.05, 0.15), vec3(0.95, 0.55, 0.25), l); // Udnie
    if (style == 18) return mix(vec3(0.05, 0.15, 0.35), vec3(0.85, 0.92, 0.98), l); // Great Wave
    return mix(vec3(0.85, 0.15, 0.1), vec3(0.95, 0.85, 0.35), l); // Fauvist Portrait
}

vec3 pvjBlurSeparable(sampler2D tex, vec2 uv, float radius)
{
    if (radius <= 1e-6) {
        return pvjSampleRgb(tex, uv);
    }
    vec3 c = pvjBlur9(tex, uv, vec2(1.0, 0.0), radius);
    return pvjBlur9(tex, uv, vec2(0.0, 1.0), radius * 0.85);
}

vec3 pvjRegionMean(sampler2D tex, vec2 uv, float radius)
{
    vec3 sum = vec3(0.0);
    const float w00 = 0.0625; const float w01 = 0.125; const float w02 = 0.0625;
    const float w10 = 0.125;  const float w11 = 0.25;  const float w12 = 0.125;
    const float w20 = 0.0625; const float w21 = 0.125; const float w22 = 0.0625;
    sum += pvjSampleRgb(tex, uv + vec2(-radius, -radius)) * w00;
    sum += pvjSampleRgb(tex, uv + vec2(0.0, -radius)) * w01;
    sum += pvjSampleRgb(tex, uv + vec2(radius, -radius)) * w02;
    sum += pvjSampleRgb(tex, uv + vec2(-radius, 0.0)) * w10;
    sum += pvjSampleRgb(tex, uv) * w11;
    sum += pvjSampleRgb(tex, uv + vec2(radius, 0.0)) * w12;
    sum += pvjSampleRgb(tex, uv + vec2(-radius, radius)) * w20;
    sum += pvjSampleRgb(tex, uv + vec2(0.0, radius)) * w21;
    sum += pvjSampleRgb(tex, uv + vec2(radius, radius)) * w22;
    // Wider ring for larger color pools.
    sum += pvjSampleRgb(tex, uv + vec2(-radius * 2.0, 0.0)) * 0.08;
    sum += pvjSampleRgb(tex, uv + vec2(radius * 2.0, 0.0)) * 0.08;
    sum += pvjSampleRgb(tex, uv + vec2(0.0, -radius * 2.0)) * 0.08;
    sum += pvjSampleRgb(tex, uv + vec2(0.0, radius * 2.0)) * 0.08;
    return sum / 1.32;
}

vec3 pvjAbstraction(sampler2D tex, vec2 uv, vec3 src)
{
    float preBlur = ubuf.params.x;
    float strength = max(ubuf.params.y, 0.001);
    float iterate = ubuf.params.z;
    float blend = ubuf.params.w;
    float quantizeOn = ubuf.params2.y;
    float steps = max(ubuf.params2.z, 2.0);
    float softness = ubuf.params2.w;
    float drawEdge = ubuf.params3.x;
    float edgeStr = ubuf.params3.y;
    float edgeThresh = ubuf.params3.z;

    float px = pvjPx();
    float preR = preBlur * px * mix(2.0, 72.0, preBlur);
    float poolR = px * mix(3.0, 96.0, strength);

    vec3 c = pvjBlurSeparable(tex, uv, preR);

    int iterCount = min(1 + int(iterate * 6.0 + strength * 2.0), 8);
    for (int k = 0; k < 8; ++k) {
        if (k >= iterCount) {
            break;
        }
        vec3 region = pvjRegionMean(tex, uv, poolR * (1.0 + float(k) * 0.45));
        c = mix(c, region, min(1.0, strength * 0.92));
    }

    float levels = quantizeOn > 0.5 ? steps : mix(18.0, 5.0, strength);
    c = floor(c * levels + 0.5) / levels;
    if (softness > 0.001) {
        vec3 soft = pvjBlurSeparable(tex, uv, px * mix(1.0, 8.0, softness));
        c = mix(c, soft, softness * 0.22);
    }

    float edgeAmt = drawEdge > 0.5 ? edgeStr : strength * 0.35;
    if (edgeAmt > 0.01) {
        vec3 e = pvjSobel(tex, uv, 0.6 + edgeStr * 4.0);
        float el = dot(e, PVJ_LUMA);
        float thresh = drawEdge > 0.5 ? edgeThresh : mix(0.12, 0.35, 1.0 - strength);
        float ink = smoothstep(thresh, thresh + 0.07, el) * edgeAmt;
        c = mix(c, c * 0.15, ink);
    }

    return mix(src, clamp(c, 0.0, 1.0), blend);
}

vec3 pvjBlankingFill(sampler2D tex, vec2 uv, vec3 src, float alpha)
{
    float blend = ubuf.params.x;
    int zoomMode = int(ubuf.params.y + 0.5);
    float expand = ubuf.params.z;
    float aspect = ubuf.params.w;
    float blendEdges = ubuf.params2.x;
    float blurBg = ubuf.params2.y;
    float fadeAmt = ubuf.params2.z;
    vec3 fadeColor = ubuf.light[0].rgb;
    float shadowStr = ubuf.params3.x;
    float dropAngle = ubuf.params3.y;
    float dropDist = ubuf.params3.z;
    float dropBlur = ubuf.params3.w;
    vec3 dropColor = ubuf.light[1].rgb;

    vec2 p = pvjUvCentered(uv);
    float frameAspect = pvjAspect();
    p.x *= frameAspect;

    // aspect param sets active picture width; outside = letterbox/pillarbox to fill
    float contentAspect = mix(0.55, 2.35, aspect);
    float activeHalfW = 0.5 * min(1.0, frameAspect / max(contentAspect, 0.001));
    float outsideX = max(abs(p.x) - activeHalfW, 0.0);

    // blend_edges = feather width at the fill boundary (0 = hard, 1 = soft)
    float feather = mix(0.002, 0.12, blendEdges) + expand * 0.02;
    float fillMask = smoothstep(0.0, feather, outsideX);

    vec2 fillUv = uv;
    if (fillMask > 0.001) {
        if (zoomMode == 2) {
            fillUv.x = uv.x + sign(p.x) * fillMask * expand * 0.25 * (1.0 + aspect);
        } else if (zoomMode == 1) {
            fillUv = mix(uv, (uv - 0.5) * (1.0 + expand * 0.8) + 0.5, fillMask);
        } else {
            float edgeX = 0.5 + sign(p.x) * (activeHalfW / frameAspect);
            vec2 mirrored = uv;
            mirrored.x = edgeX - (uv.x - edgeX);
            fillUv = mix(uv, mirrored, fillMask);
        }
    }

    vec3 fill = pvjSampleRgb(tex, fillUv);
    if (blurBg > 0.01) {
        fill = mix(fill, pvjBlur9(tex, fillUv, vec2(1.0), blurBg * 0.015), fillMask);
    }
    fill = mix(fill, fadeColor, fadeAmt * fillMask);

    vec3 fx = mix(src, fill, fillMask);
    vec2 off = vec2(cos(dropAngle), sin(dropAngle)) * dropDist;
    vec3 shadow = dropColor * shadowStr * alpha;
    fx = mix(fx, shadow, smoothstep(0.0, dropBlur * 0.05, length(p - off * 2.0)) * shadowStr * 0.3);
    return mix(src, fx, blend);
}

vec3 pvjDropShadow(sampler2D tex, vec2 uv, vec3 src, float alpha)
{
    float strength = ubuf.params.x;
    float angle = ubuf.params.y;
    float dist = ubuf.params.z;
    float blur = ubuf.params.w;
    vec3 color = ubuf.light[0].rgb;
    float blend = ubuf.params2.x;
    vec2 off = vec2(cos(angle), sin(angle)) * dist;
    vec3 shadowBase = color * strength;
    vec3 blurred = shadowBase;
    if (blur > 0.01) {
        blurred = vec3(0.0);
        for (int i = -2; i <= 2; ++i)
            blurred += shadowBase * alpha;
        blurred /= 5.0;
    }
    vec3 fx = src + blurred * alpha * strength;
    return mix(src, fx, blend);
}

vec3 pvjEdgeDetect(sampler2D tex, vec2 uv, vec3 src)
{
    int mode = int(ubuf.params.x + 0.5);
    float thickness = ubuf.params.y;
    float threshold = ubuf.params.z;
    float glow = ubuf.params.w;
    vec3 edgeColor = ubuf.light[0].rgb;
    float blend = ubuf.params2.x;

    vec3 e = pvjSobel(tex, uv, 0.5 + thickness * 4.0);
    float el = length(e);
    if (mode == 1) {
        el = abs(dot(e, PVJ_LUMA));
    }
    el = smoothstep(threshold, threshold + 0.15, el);

    // edge_color tints edges in both modes; RGB keeps some source chroma
    vec3 chroma = clamp(abs(e) * 2.0, 0.0, 1.0);
    vec3 fx = edgeColor * el;
    if (mode == 0) {
        fx = mix(chroma * el, edgeColor * el, 0.72);
    }
    if (glow > 0.01) {
        fx += pvjBlur9(tex, uv, vec2(1.0), glow * 0.008) * glow * el;
    }
    return mix(src, fx, blend);
}

vec3 pvjEmboss(sampler2D tex, vec2 uv, vec3 src)
{
    int style = int(ubuf.params.x + 0.5);
    float power = ubuf.params.y;
    float angle = ubuf.params.z;
    float blend = ubuf.params.w;
    vec2 dir = vec2(cos(angle), sin(angle)) * pvjPx() * (1.0 + power * 4.0);
    vec3 a = pvjSampleRgb(tex, uv + dir);
    vec3 b = pvjSampleRgb(tex, uv - dir);
    vec3 fx;
    if (style == 0) fx = (a - b) * power * 2.0 + 0.5;
    else if (style == 1) fx = src + (a - b) * power * 2.0;
    else if (style == 2) fx = pvjSobel(tex, uv, 1.0 + power * 2.0);
    else fx = abs(pvjSobel(tex, uv, 1.5)) * power * 2.0;
    return mix(src, clamp(fx, 0.0, 1.0), blend);
}

vec3 pvjMirrors(sampler2D tex, vec2 uv, vec3 src)
{
    int placement = int(ubuf.params.x + 0.5);
    bool reflectBorders = ubuf.params.y > 0.5;
    float blend = ubuf.params.z;
    vec2 uvOut = uv;

    if (placement == 1) {
        vec2 center = vec2(ubuf.light[0].x, ubuf.light[0].y);
        uvOut = pvjRosetteUv(uv, center, ubuf.light[0].z, ubuf.light[0].w);
    } else if (placement == 2) {
        vec2 center = vec2(ubuf.light[1].x, ubuf.light[1].y);
        uvOut = pvjKaleidoUv(uv, center, ubuf.light[1].z, ubuf.light[1].w, ubuf.light[2].x);
    } else {
        for (int i = 0; i < 6; ++i) {
            if (ubuf.light[3 + i].x < 0.5) continue;
            vec2 center = vec2(ubuf.light[3 + i].y, ubuf.light[3 + i].z);
            bool flip = ubuf.params3[i] > 0.5;
            uvOut = pvjMirrorUv(uvOut, center, ubuf.light[3 + i].w, flip);
        }
    }
    if (reflectBorders)
        uvOut = clamp(uvOut, vec2(0.001), vec2(0.999));
    vec3 fx = pvjSampleRgb(tex, uvOut);
    return mix(src, fx, blend);
}

vec3 pvjPencilSketch(sampler2D tex, vec2 uv, vec3 src)
{
    bool colorSketch = ubuf.params.x > 0.5;
    float thickness = ubuf.params.y;
    float threshold = ubuf.params.z;
    float strokeLen = ubuf.params.w;
    float toneLevels = ubuf.params2.x * 16.0;
    float shadows = ubuf.params2.y;
    float midtones = ubuf.params2.z;
    float highlights = ubuf.params2.w;
    float texAmt = ubuf.params3.x;
    float texScale = ubuf.params3.y;
    bool animate = ubuf.params3.z > 0.5;
    float blend = ubuf.params3.w;
    float t = animate ? pvjTime() : 0.0;

    vec3 e = pvjSobel(tex, uv, 0.5 + thickness * 3.0);
    float el = dot(e, PVJ_LUMA);
    float l = dot(src, PVJ_LUMA);
    l = mix(l, l * shadows, 1.0 - shadows);
    l = mix(l, l * midtones, 0.5);
    l = mix(l, l * highlights, highlights);
    l = floor(l * toneLevels) / toneLevels;
    float stroke = 1.0 - smoothstep(threshold, threshold + 0.2 + strokeLen * 0.3, el);
    vec3 fx = colorSketch ? src * l : vec3(l);
    fx *= stroke;
    fx += (pvjNoise(uv * texScale * 200.0 + t) - 0.5) * texAmt * 0.3;
    return mix(src, clamp(fx, 0.0, 1.0), blend);
}

vec3 pvjPrismBlur(sampler2D tex, vec2 uv, vec3 src)
{
    float blurStr = ubuf.params.x;
    float aber = ubuf.params.y;
    float vigSize = ubuf.params.z;
    float vigSharp = ubuf.params.w;
    float blend = ubuf.params2.x;
    float r = blurStr * 0.012;
    vec3 c;
    c.r = pvjSampleRgb(tex, uv + vec2(r * aber, 0.0)).r;
    c.g = pvjSampleRgb(tex, uv).g;
    c.b = pvjSampleRgb(tex, uv - vec2(r * aber, 0.0)).b;
    c = mix(c, pvjBlur9(tex, uv, vec2(1.0), r), blurStr);
    float d = length(pvjUvCentered(uv));
    float vig = smoothstep(vigSize * 0.5, vigSize * 0.5 + (1.0 - vigSharp) * 0.3 + 0.05, d);
    c *= 1.0 - vig * 0.6;
    return mix(src, c, blend);
}

vec3 pvjScanlines(sampler2D tex, vec2 uv, vec3 src)
{
    float freq = ubuf.params.x;
    float sharpness = ubuf.params.y;
    float angle = ubuf.params.z;
    float width = ubuf.params.w;
    float shift = ubuf.params2.x;
    float blend = ubuf.params2.y;
    vec3 color1 = ubuf.light[0].rgb;
    vec3 color2 = ubuf.light[1].rgb;

    vec2 p = uv - 0.5;
    float c = cos(angle);
    float s = sin(angle);
    float line = p.x * s + p.y * c;
    line = line * freq + shift;
    float lineFrac = fract(line);
    float w = mix(0.5, 0.05, width);
    float mask = smoothstep(w, w + (1.0 - sharpness) * 0.2, lineFrac);
    vec3 lineColor = mix(color1, color2, step(0.5, lineFrac));
    vec3 fx = mix(src, src * (1.0 - mask * 0.7) + lineColor * mask * 0.5, 1.0);
    return mix(src, fx, blend);
}

vec3 pvjStylizeEffect(sampler2D tex, vec2 uv, vec3 src)
{
    int style = int(ubuf.params.x + 0.5);
    float scale = ubuf.params.y;
    float blend = ubuf.params.z;
    float r = 0.003 + scale * 0.012;
    vec3 blur = pvjBlur9(tex, uv, vec2(1.0), r);
    vec3 e = pvjSobel(tex, uv, 1.0 + scale);
    float el = dot(e, PVJ_LUMA);
    vec3 base = mix(blur, pvjStylePalette(style, blur), 0.85);
    base = mix(base, base * (1.0 - el * 0.5), 0.4);
    base += pvjNoise(floor(uv * (20.0 + scale * 80.0)) / (20.0 + scale * 80.0)) * 0.03;
    return mix(src, clamp(base, 0.0, 1.0), blend);
}

vec3 pvjTiltShift(sampler2D tex, vec2 uv, vec3 src)
{
    int blurType = int(ubuf.params.x + 0.5);
    float blurR = ubuf.params.y;
    int irisShape = int(ubuf.params.z + 0.5);
    float irisBlades = ubuf.params.w;
    float focusCenter = ubuf.params2.x;
    float focusWidth = ubuf.params2.y;
    float focusAngle = ubuf.params2.z;
    float blend = ubuf.params2.w;

    vec2 p = pvjUvCentered(uv);
    p.x *= pvjAspect();
    float c = cos(focusAngle);
    float s = sin(focusAngle);
    float bandDist = abs(p.x * s + p.y * c - (focusCenter - 0.5));
    float focus = smoothstep(focusWidth * 0.5, focusWidth * 0.5 + 0.15, bandDist);

    float r = blurR * (blurType == 1 ? 1.5 : 1.0);
    vec3 blurred;
    if (blurType == 1) {
        int bokehSides = (irisShape == 0) ? pvjIrisSides(0, irisBlades)
                                          : pvjIrisSides(irisShape, irisBlades);
        blurred = pvjLensIrisBlur(tex, uv, r, bokehSides);
    } else {
        blurred = pvjBlur9(tex, uv, vec2(1.0, 0.0), r);
        blurred = mix(blurred, pvjBlur9(tex, uv, vec2(0.0, 1.0), r), 0.5);
    }
    return mix(src, mix(src, blurred, focus), blend);
}

vec3 pvjVignette(vec2 uv, vec3 src)
{
    float size = ubuf.params.x;
    float softness = ubuf.params.y;
    float strength = ubuf.params.z;
    float roundness = ubuf.params.w;
    float cx = ubuf.params2.x;
    float cy = ubuf.params2.y;
    float blend = ubuf.params2.z;
    vec3 color = ubuf.light[0].rgb;

    vec2 p = uv - vec2(cx, cy);
    p.x *= mix(pvjAspect(), 1.0, roundness);
    float d = length(p);
    float vig = smoothstep(size * 0.5, size * 0.5 + softness * 0.4 + 0.01, d);
    vec3 fx = mix(src, color, vig * strength);
    return mix(src, fx, blend);
}

vec3 pvjWatercolor(sampler2D tex, vec2 uv, vec3 src)
{
    float detail = ubuf.params.x;
    float brush = ubuf.params.y;
    float edgeDark = ubuf.params.z;
    float paper = ubuf.params.w;
    float blend = ubuf.params2.x;

    float r = brush * 0.015 + (1.0 - detail) * 0.008;
    vec3 c = pvjBlur9(tex, uv, vec2(1.0), r);
    c = mix(c, pvjBlur9(tex, uv, vec2(0.7, 0.7), r * 0.7), 0.4);
    vec3 e = pvjSobel(tex, uv, 1.0 + detail);
    float el = dot(e, PVJ_LUMA);
    c *= 1.0 - el * edgeDark;
    c += (pvjNoise(uv * 120.0) - 0.5) * paper * 0.15;
    return mix(src, clamp(c, 0.0, 1.0), blend);
}

void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 rgb = src;
    int id = pvjEffectId();

    if (id == 0) rgb = pvjAbstraction(u_tex, uv, src);
    else if (id == 1) rgb = pvjBlankingFill(u_tex, uv, src, tex.a);
    else if (id == 2) rgb = pvjDropShadow(u_tex, uv, src, tex.a);
    else if (id == 3) rgb = pvjEdgeDetect(u_tex, uv, src);
    else if (id == 4) rgb = pvjEmboss(u_tex, uv, src);
    else if (id == 5) rgb = pvjMirrors(u_tex, uv, src);
    else if (id == 6) rgb = pvjPencilSketch(u_tex, uv, src);
    else if (id == 7) rgb = pvjPrismBlur(u_tex, uv, src);
    else if (id == 8) rgb = pvjScanlines(u_tex, uv, src);
    else if (id == 9) rgb = pvjStylizeEffect(u_tex, uv, src);
    else if (id == 10) rgb = pvjTiltShift(u_tex, uv, src);
    else if (id == 11) rgb = pvjVignette(uv, src);
    else if (id == 12) rgb = pvjWatercolor(u_tex, uv, src);

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
