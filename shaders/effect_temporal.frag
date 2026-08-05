#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;
layout(binding = 2) uniform sampler2D u_history;

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
float pvjPresentFrame() { return ubuf.scaleOffset.z; }
float pvjAspect() { return max(ubuf.scaleOffset.w, 0.001); }

vec2 pvjUvCentered(vec2 uv) { return uv - 0.5; }
vec2 pvjUvFromCentered(vec2 p) { return p + 0.5; }

vec3 pvjSampleRgb(sampler2D tex, vec2 uv)
{
    return texture(tex, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
}

vec4 pvjSampleRgba(sampler2D tex, vec2 uv)
{
    return texture(tex, clamp(uv, vec2(0.001), vec2(0.999)));
}

float pvjLuma(vec3 c) { return dot(c, PVJ_LUMA); }

float pvjSaturation(vec3 c)
{
    float maxc = max(max(c.r, c.g), c.b);
    float minc = min(min(c.r, c.g), c.b);
    return maxc - minc;
}

vec2 pvjTrailOffset(int copyIndex, float panAmt, float panAngle, float zoomAmt, float rotateAmt)
{
    float dist = panAmt * float(copyIndex) * 0.02;
    vec2 dir = vec2(cos(panAngle), sin(panAngle));
    return dir * dist;
}

vec2 pvjApplyTrailTransform(vec2 uv, int copyIndex, float panAmt, float panAngle,
                            float zoomAmt, float rotateAmt)
{
    vec2 p = pvjUvCentered(uv);
    float ang = rotateAmt * float(copyIndex);
    float c = cos(ang);
    float s = sin(ang);
    p = vec2(c * p.x - s * p.y, s * p.x + c * p.y);
    float scale = 1.0 + zoomAmt * float(copyIndex) * 0.05;
    p /= max(scale, 0.01);
    p += pvjTrailOffset(copyIndex, panAmt, panAngle, zoomAmt, rotateAmt);
    return pvjUvFromCentered(p);
}

vec3 pvjGammaEncode(vec3 c, float gamma)
{
    return pow(max(c, vec3(0.0)), vec3(1.0 / max(gamma, 0.01)));
}

vec3 pvjGammaDecode(vec3 c, float gamma)
{
    return pow(max(c, vec3(0.0)), vec3(max(gamma, 0.01)));
}

vec3 pvjCompositeOver(vec3 under, vec3 over, float alpha)
{
    return mix(under, over, clamp(alpha, 0.0, 1.0));
}

vec3 pvjResolveGammaComposite(vec3 acc, vec3 layer, float alpha, int gammaMode, float customGamma)
{
    float gamma = 2.4;
    if (gammaMode == 1) {
        gamma = 2.2;
    } else if (gammaMode == 2) {
        gamma = 1.0;
    } else if (gammaMode == 3) {
        gamma = max(customGamma, 0.01);
    }
    vec3 a = pvjGammaDecode(acc, gamma);
    vec3 b = pvjGammaDecode(layer, gamma);
    vec3 m = mix(a, b, alpha);
    return pvjGammaEncode(m, gamma);
}

vec3 pvjBorderSample(sampler2D tex, vec2 uv, int borderType)
{
    if (borderType == 0) {
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            return vec3(0.0);
        }
        return pvjSampleRgb(tex, uv);
    }
    if (borderType == 1) {
        vec2 c = clamp(uv, vec2(0.0), vec2(1.0));
        vec3 col = pvjSampleRgb(tex, c);
        float edge = max(max(abs(uv.x - 0.5) - 0.48, abs(uv.y - 0.5) - 0.48), 0.0);
        return mix(col, vec3(0.0), clamp(edge * 8.0, 0.0, 1.0));
    }
    if (borderType == 2 || borderType == 3) {
        vec2 c = clamp(uv, vec2(0.0), vec2(1.0));
        return pvjSampleRgb(tex, c);
    }
    vec2 w = fract(uv);
    return pvjSampleRgb(tex, w);
}

vec3 pvjMotionTrails(vec2 uv, vec3 src, vec4 srcTex)
{
    const int trailLen = int(clamp(ubuf.params[0], 1.0, 16.0));
    const float dropoff = max(ubuf.params[1], 0.01);
    const float blend = ubuf.params[2];
    const float panAmt = ubuf.params[3];
    const float panAngle = ubuf.params2[1];
    const float zoomAmt = ubuf.params2[2];
    const float rotateAmt = ubuf.params2[3];
    const bool reuseCurrent = ubuf.params3[0] > 0.5;
    const int gammaMode = int(ubuf.params3[1] + 0.5);
    const float customGamma = ubuf.params3[2];
    const int borderType = int(ubuf.params3[3] + 0.5);

    vec3 acc = src;
    float weightSum = 1.0;

    for (int i = 1; i <= trailLen; ++i) {
        vec2 tuv = reuseCurrent ? uv : pvjApplyTrailTransform(uv, i, panAmt, panAngle, zoomAmt, rotateAmt);
        vec3 layer = pvjBorderSample(u_history, tuv, borderType);
        float w = pow(dropoff, float(i));
        acc = pvjResolveGammaComposite(acc, layer, w, gammaMode, customGamma);
        weightSum += w;
    }

    vec3 fx = acc / max(weightSum, 1e-4);
    return mix(src, fx, blend);
}

vec3 pvjSmear(vec2 uv, vec3 src, vec4 srcTex)
{
    const int framesSide = int(clamp(ubuf.params[0], 0.0, 8.0));
    const float lumaThr = ubuf.params[1];
    const float chromaThr = ubuf.params[2];
    const float blend = ubuf.params[3];

    vec3 hist = pvjSampleRgb(u_history, uv);
    vec2 motion = src.xy - hist.xy;
    float motionLen = length(motion);

    vec3 acc = src;
    float wSum = 1.0;

    for (int i = -framesSide; i <= framesSide; ++i) {
        if (i == 0) {
            continue;
        }
        float t = float(i) / float(max(framesSide, 1));
        vec2 off = motion * t * 0.5;
        vec3 sampleRgb = (i < 0) ? pvjSampleRgb(u_history, uv + off) : pvjSampleRgb(u_tex, uv - off);
        float luma = pvjLuma(sampleRgb);
        float sat = pvjSaturation(sampleRgb);
        float mask = step(lumaThr, luma) * step(chromaThr, sat);
        float w = 1.0 / (1.0 + abs(float(i)));
        acc += sampleRgb * w * mask;
        wSum += w * mask;
    }

    vec3 fx = acc / max(wSum, 1e-4);
    return mix(src, fx, blend);
}

vec3 pvjStopMotion(vec2 uv, vec3 src, vec4 srcTex)
{
    const float holdFrames = max(ubuf.params[0], 1.0);
    const float blend = ubuf.params[1];
    const int inputAlpha = int(ubuf.light[0][0] + 0.5);
    const bool useAlpha = ubuf.light[0][1] > 0.5;

    const float phase = mod(pvjPresentFrame(), holdFrames);
    vec3 held = (phase < 0.5) ? src : pvjSampleRgb(u_history, uv);

    vec3 fx = held;
    if (useAlpha && inputAlpha == 1) {
        fx = mix(src, held, srcTex.a);
    }
    return mix(src, fx, blend);
}

vec3 pvjMotionBlur(vec2 uv, vec3 src, vec4 srcTex)
{
    const float amount = ubuf.params[0];
    const float motionRange = ubuf.params[1];
    const float granularity = ubuf.params[2];
    const float blend = ubuf.params[3];
    const bool better = ubuf.params2[1] < 0.5;
    const int direction = int(ubuf.params2[2] + 0.5);

    vec3 hist = pvjSampleRgb(u_history, uv);
    vec2 vel = (src - hist).xy * 0.5;
    float speed = length(vel);
    if (speed < motionRange * 0.002) {
        return src;
    }
    vel = normalize(vel + 1e-6) * speed * amount * 0.08;

    int samples = better ? int(mix(6.0, 16.0, granularity)) : int(mix(3.0, 8.0, granularity));
    vec3 acc = vec3(0.0);
    float wSum = 0.0;
    for (int i = 0; i < samples; ++i) {
        float t = float(i) / float(max(samples - 1, 1)) - 0.5;
        if (direction == 1 && t > 0.0) {
            continue;
        }
        if (direction == 2 && t < 0.0) {
            continue;
        }
        vec2 off = vel * t;
        vec3 s = (t <= 0.0) ? pvjSampleRgb(u_history, uv + off) : pvjSampleRgb(u_tex, uv + off);
        float w = better ? exp(-4.0 * t * t) : 1.0;
        acc += s * w;
        wSum += w;
    }
    vec3 fx = acc / max(wSum, 1e-4);
    return mix(src, fx, blend * clamp(speed * 40.0, 0.0, 1.0));
}

void main()
{
    vec2 uv = v_uv;
    vec4 tex = pvjSampleRgba(u_tex, uv);
    vec3 src = tex.rgb;
    vec3 fx = src;
    const int id = pvjEffectId();

    if (id == 0) {
        fx = pvjMotionTrails(uv, src, tex);
    } else if (id == 1) {
        fx = pvjSmear(uv, src, tex);
    } else if (id == 2) {
        fx = pvjStopMotion(uv, src, tex);
    } else if (id == 3) {
        fx = pvjMotionBlur(uv, src, tex);
    }

    fragColor = vec4(clamp(fx, 0.0, 1.0), tex.a);
}
