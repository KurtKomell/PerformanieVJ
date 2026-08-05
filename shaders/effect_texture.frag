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

vec3 pvjBlurSeparable(sampler2D tex, vec2 uv, float r)
{
    vec3 h = pvjBlur9(tex, uv, vec2(1.0, 0.0), r);
    vec3 v = pvjBlur9(tex, uv, vec2(0.0, 1.0), r);
    return mix(h, v, 0.5);
}

vec3 pvjApplyTextureBand(sampler2D tex, vec2 uv, vec3 texRgb, vec3 accum, float rad,
                       float textureVal)
{
    if (abs(textureVal) < 0.001) {
        return accum;
    }
    vec3 blur = pvjBlurSeparable(tex, uv, rad);
    vec3 detail = texRgb - blur;
    return accum + detail * textureVal * 0.5;
}

float pvjTonalWeight(float luma, float shadows, float midtones, float highlights)
{
    float sh = smoothstep(0.0, 0.35, luma) * shadows;
    float mid = smoothstep(0.2, 0.45, luma) * (1.0 - smoothstep(0.55, 0.8, luma)) * midtones;
    float hi = smoothstep(0.65, 1.0, luma) * highlights;
    return clamp(sh + mid + hi, 0.0, 1.0);
}

// --- JPEG Damage (familyId 0) ---
vec3 pvjJpegDamage(sampler2D tex, vec2 uv, vec3 src)
{
    float quality = ubuf.params.x;
    float resolution = ubuf.params.y;
    float blockAspect = ubuf.params.z;
    float freqScale = ubuf.params2.x;
    int scaleComp = int(ubuf.params2.y + 0.5);
    float blend = ubuf.params2.z;

    float blockSize = mix(2.0, 48.0, resolution) * pvjPx();
    vec2 blockUv = vec2(blockSize, blockSize * mix(0.5, 2.0, blockAspect));
    if (scaleComp == 1) {
        blockUv.y *= 0.35;
    } else if (scaleComp == 2) {
        blockUv.x *= 0.35;
    }
    vec2 grid = floor(uv / blockUv) * blockUv + blockUv * 0.5;
    vec3 blockCol = pvjSampleRgb(tex, grid);
    vec3 quant = floor(blockCol * mix(4.0, 32.0, quality) + 0.5) / mix(4.0, 32.0, quality);
    vec2 edge = abs(fract(uv / blockUv) - 0.5) * 2.0;
    float edgeArt = max(edge.x, edge.y);
    edgeArt = pow(edgeArt, mix(2.0, 0.4, freqScale));
    vec3 damaged = mix(quant, src, edgeArt * 0.35);
    damaged = mix(damaged, quant, (1.0 - quality) * 0.65);
    return mix(src, damaged, blend);
}

// --- Texture Pop (familyId 1) ---
vec3 pvjTexturePop(sampler2D tex, vec2 uv, vec3 src)
{
    int mode = int(ubuf.params.x + 0.5);
    float strength = ubuf.params.y;
    float blend = ubuf.params.z;
    float shadows = ubuf.params.w;
    float midtones = ubuf.params2.x;
    float highlights = ubuf.params2.y;
    float luma = dot(src, PVJ_LUMA);
    float tonal = pvjTonalWeight(luma, shadows, midtones, highlights);

    vec3 result = src;
    if (mode == 0) {
        float details = ubuf.params2.z;
        float r = 0.008 + pvjPx() * 4.0;
        result = pvjApplyTextureBand(tex, uv, src, result, r, details * strength);
    } else {
        float rough = ubuf.params3.x;
        float coarse = ubuf.params3.y;
        float medium = ubuf.params3.z;
        float small = ubuf.light[0].x;
        float fine = ubuf.light[0].y;
        float tiny = ubuf.light[0].z;
        float px = pvjPx();
        result = pvjApplyTextureBand(tex, uv, src, result, px * 64.0, rough * strength);
        result = pvjApplyTextureBand(tex, uv, src, result, px * 32.0, coarse * strength);
        result = pvjApplyTextureBand(tex, uv, src, result, px * 16.0, medium * strength);
        result = pvjApplyTextureBand(tex, uv, src, result, px * 8.0, small * strength);
        result = pvjApplyTextureBand(tex, uv, src, result, px * 4.0, fine * strength);
        result = pvjApplyTextureBand(tex, uv, src, result, px * 2.0, tiny * strength);
    }
    vec3 fx = mix(src, clamp(result, 0.0, 1.0), tonal);
    return mix(src, fx, blend);
}

// --- Film Damage (familyId 2) ---
float pvjFilmVignette(vec2 uv, float focal, float geometry, float tiltAmt, float tiltAng)
{
    vec2 p = uv - 0.5;
    p.x *= pvjAspect();
    float c = cos(tiltAng);
    float s = sin(tiltAng);
    p = vec2(p.x * c + p.y * s, -p.x * s + p.y * c);
    p.y += tiltAmt * 0.25;
    float d = length(p);
    return smoothstep(focal * 0.35 + 0.05, focal * 0.35 + geometry * 0.5 + 0.2, d);
}

vec3 pvjFilmDirt(vec2 uv, vec3 src, float density, float size, float blur, float seed,
                 bool changing, vec3 dirtColor)
{
    if (density < 0.001) {
        return src;
    }
    float t = changing ? pvjTime() : 0.0;
    vec2 cell = floor(uv * mix(80.0, 20.0, size) + seed * 17.3 + t * 3.1);
    float n = pvjHash(cell);
    if (n > density * 0.15) {
        return src;
    }
    vec2 spot = fract(uv * mix(120.0, 40.0, size) + cell) - 0.5;
    float spotR = length(spot) / mix(0.08, 0.02, size);
    float m = smoothstep(1.0, 1.0 - blur * 0.8, spotR);
    return mix(src, dirtColor, m * 0.85);
}

float pvjFilmScratch(vec2 uv, int slot, float pos, float width, float str, float blur,
                     bool moving, float amp, float speed, float randness, float flickerSpd)
{
    float t = pvjTime();
    float x = pos;
    if (moving) {
        x += sin(t * speed * 6.0 + float(slot) * 2.1) * amp * 0.08;
        x += pvjNoise(vec2(t * randness * 2.0, float(slot))) * amp * 0.04;
    }
    float flick = 0.7 + 0.3 * sin(t * flickerSpd * 12.0 + float(slot));
    float dx = abs(uv.x - x);
    float line = smoothstep(width * 0.5 + blur * 0.02, width * 0.5, dx);
    return line * str * flick;
}

vec3 pvjFilmDamage(sampler2D tex, vec2 uv, vec3 src)
{
    float filmBlur = ubuf.light[0].x;
    float tempShift = ubuf.light[0].y;
    float tintShift = ubuf.light[0].z;
    float blend = ubuf.light[0].w;
    float focal = ubuf.light[1].x;
    float geometry = ubuf.light[1].y;
    float tiltAmt = ubuf.light[1].z;
    float tiltAng = ubuf.light[1].w;
    float dirtDensity = ubuf.light[2].x;
    float dirtSize = ubuf.light[2].y;
    float dirtBlur = ubuf.light[2].z;
    float dirtSeed = ubuf.light[2].w;
    bool changingDirt = ubuf.light[3].w > 0.5;
    vec3 dirtColor = ubuf.light[3].rgb;

    vec3 c = src;
    if (filmBlur > 0.001) {
        c = mix(c, pvjBlurSeparable(tex, uv, filmBlur * 0.02 + pvjPx() * 2.0), filmBlur);
    }
    c.r += tempShift * 0.08;
    c.b -= tempShift * 0.08;
    c.r += tintShift * 0.06;
    c.b -= tintShift * 0.04;
    c.g -= tintShift * 0.02;

    float vig = pvjFilmVignette(uv, focal, geometry, tiltAmt, tiltAng);
    c *= mix(1.0, 0.25 + geometry * 0.5, vig);

    c = pvjFilmDirt(uv, c, dirtDensity, dirtSize, dirtBlur, dirtSeed, changingDirt, dirtColor);

    float randness = ubuf.params3.w;
    for (int i = 0; i < 5; ++i) {
        vec4 s0 = ubuf.light[4 + i * 2];
        vec4 s1 = ubuf.light[5 + i * 2];
        float amp = (i < 2) ? ubuf.light[14][i * 2] : ubuf.light[15][(i - 2) * 2];
        float spd = (i < 2) ? ubuf.light[14][i * 2 + 1] : ubuf.light[15][(i - 2) * 2 + 1];
        float flick = (i == 0) ? ubuf.params3.x
                    : (i == 1) ? ubuf.params3.y
                    : (i == 2) ? ubuf.params3.z
                    : (i == 3) ? ubuf.light[15].z : ubuf.light[15].w;
        float scratch = pvjFilmScratch(uv, i, s0.x, s0.y, s0.z, s0.w, s1.w > 0.5, amp, spd,
                                       randness, flick);
        c = mix(c, s1.rgb, scratch);
    }

    return mix(src, clamp(c, 0.0, 1.0), blend);
}

// --- Analog Damage (familyId 3) ---
vec3 pvjAnalogDamage(sampler2D tex, vec2 uv, vec3 src)
{
    float blend = ubuf.params.w;
    float vignetting = ubuf.light[0].x;
    float vignetteAspect = ubuf.light[0].y;
    float shutterWeave = ubuf.light[0].z;
    float noiseScale = ubuf.light[1].x;
    float signalNoise = ubuf.light[1].y;
    float chromaNoise = ubuf.light[1].z;
    float detailLoss = ubuf.light[1].w;
    float chromaDetailLoss = ubuf.light[2].x;
    float ghosting = ubuf.light[2].y;
    float ghostOffset = ubuf.light[2].z;
    float chromaMis = ubuf.light[2].w;
    float brightness = ubuf.light[3].x;
    float contrast = ubuf.light[3].y;
    float colorDial = ubuf.light[3].z;
    float tint = ubuf.light[3].w;
    float imageAspect = ubuf.light[4].x;
    float hShift = ubuf.light[4].y;
    float vShift = ubuf.light[4].z;
    float vHold = ubuf.light[4].w;
    float overscan = ubuf.light[5].x;
    float vScale = ubuf.light[5].y;
    float vertBlank = ubuf.light[5].z;
    float lineSharp = ubuf.light[6].x;
    float lineFreq = ubuf.light[6].y;
    bool coloredLines = ubuf.light[6].z > 0.5;
    float phosphorBright = ubuf.light[7].x;
    float phosphorTint = ubuf.light[7].y;
    float defocus = ubuf.light[7].z;
    float screenCurve = ubuf.light[7].w;
    bool edgeMask = ubuf.light[8].x > 0.5;
    float maskCurve = ubuf.light[8].y;
    float maskAspect = ubuf.light[8].z;
    float restlessH = ubuf.light[9].x;
    float restlessOff = ubuf.light[9].y;
    float restlessJit = ubuf.light[9].z;

    vec2 p = uv - 0.5;
    p.x *= mix(pvjAspect(), 1.0, imageAspect);
    float weave = sin(pvjTime() * 4.0) * shutterWeave * 0.01;
    vec2 uv2 = uv + vec2(weave + hShift * 0.05, vShift * 0.05 + vHold * sin(pvjTime() * 2.0) * 0.02);
    uv2.y = (uv2.y - 0.5) * (1.0 + vScale * 0.15) + 0.5;
    uv2 = mix(uv2, uv, overscan * 0.3);

  vec3 c = pvjSampleRgb(tex, uv2);
    float rOff = chromaMis * 0.012;
    vec3 chroma = vec3(
        pvjSampleRgb(tex, uv2 + vec2(rOff, 0.0)).r,
        c.g,
        pvjSampleRgb(tex, uv2 - vec2(rOff, 0.0)).b);
    c = mix(c, chroma, chromaMis);

    if (defocus > 0.001 || detailLoss > 0.001) {
        vec3 blur = pvjBlurSeparable(tex, uv2, defocus * 0.025 + detailLoss * 0.015);
        c = mix(c, blur, detailLoss * 0.7 + defocus * 0.5);
        vec3 cBlur = pvjBlurSeparable(tex, uv2 + vec2(chromaMis * 0.01, 0.0), chromaDetailLoss * 0.02);
        c.gb = mix(c.gb, cBlur.gb, chromaDetailLoss);
    }

    if (ghosting > 0.001) {
        vec3 ghost = pvjSampleRgb(tex, uv2 + vec2(ghostOffset * 0.03, 0.0));
        c = mix(c, ghost, ghosting * 0.45);
    }

    float n = pvjNoise(uv2 * mix(400.0, 40.0, noiseScale) + pvjTime() * 60.0);
    c += (n - 0.5) * signalNoise * 0.25;
    c.rg += (pvjNoise(uv2 * 80.0 + 1.7) - 0.5) * chromaNoise * 0.2;

    c = (c - 0.5) * (1.0 + contrast * 0.8) + 0.5 + brightness * 0.15;
    float sat = 1.0 + colorDial * 0.6;
    float l = dot(c, PVJ_LUMA);
    c = mix(vec3(l), c, sat);
    c.r += tint * 0.08;
    c.b -= tint * 0.08;

    float scanY = fract(uv.y * lineFreq * 0.5);
    float scan = smoothstep(lineSharp * 0.5, lineSharp * 0.5 + 0.02, abs(scanY - 0.5));
    vec3 scanCol = coloredLines ? vec3(0.9, 1.0, 0.85) : vec3(0.0);
    c = mix(c, scanCol, scan * 0.35);

    vec2 crtP = uv - 0.5;
    crtP.x *= pvjAspect();
    float curve = length(crtP) * (1.0 + screenCurve * 0.5);
    float crtVig = smoothstep(0.65, 0.35 + maskCurve * 0.2, curve);
    c *= crtVig;
    c += phosphorBright * 0.05;
    c.r += phosphorTint * 0.04;
    c.b -= phosphorTint * 0.03;

    vec2 vigP = uv - 0.5;
    vigP.x *= mix(pvjAspect(), 1.0, vignetteAspect);
    float teleVig = smoothstep(0.2, 0.85, length(vigP) * 1.4);
    c *= mix(1.0, 0.4, teleVig * vignetting);

    if (edgeMask) {
        float mask = smoothstep(0.55, 0.45 - maskAspect * 0.1, curve);
        c *= mask;
    }

    float footY = 1.0 - restlessH;
    if (uv.y > footY) {
        float j = pvjNoise(vec2(pvjTime() * restlessJit * 20.0, uv.x * 10.0)) * restlessOff * 0.05;
        c = pvjSampleRgb(tex, vec2(uv.x + j, uv.y));
        c *= 0.85;
    }

    if (vertBlank > 0.001) {
        float blank = step(1.0 - vertBlank * 0.08, uv.y);
        c = mix(c, vec3(0.0), blank * 0.5);
    }

    return mix(src, clamp(c, 0.0, 1.0), blend);
}

void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 rgb = src;
    int id = pvjEffectId();

    if (id == 0) {
        rgb = pvjJpegDamage(u_tex, uv, src);
    } else if (id == 1) {
        rgb = pvjTexturePop(u_tex, uv, src);
    } else if (id == 2) {
        rgb = pvjFilmDamage(u_tex, uv, src);
    } else if (id == 3) {
        rgb = pvjAnalogDamage(u_tex, uv, src);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), tex.a);
}
