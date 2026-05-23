#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex0;
layout(binding = 2) uniform sampler2D u_tex1;
layout(binding = 3) uniform sampler2D u_tex2;
layout(binding = 4) uniform sampler2D u_tex3;
layout(binding = 5) uniform sampler2D u_tex4;
layout(binding = 6) uniform sampler2D u_tex5;
layout(binding = 7) uniform sampler2D u_tex6;
layout(binding = 8) uniform sampler2D u_tex7;
layout(binding = 9) uniform sampler2D u_tex8;
layout(binding = 10) uniform sampler2D u_tex9;
layout(binding = 11) uniform sampler2D u_tex10;
layout(binding = 12) uniform sampler2D u_tex11;
layout(binding = 13) uniform sampler2D u_under;
layout(binding = 14) uniform sampler2D u_aboveKey;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 layers[12];
    // Retained per-layer picture parameters (currently unused by the mixer
    // fragment path; kept in the UBO so the binding layout stays stable).
    vec4 picUvA[12];
    vec4 picColor[12];
    vec4 mixerCfg; // x=maxLayerExclusive, y=feedbackLayer or minLayer, z=feedbackActive, w=feedbackKeyFromAbove
} ubuf;

// Mode indices must match pvj::core::CopyMode (Model.h) exactly: 0–50
const int MODE_NORMAL              = 0;
const int MODE_ADD                 = 1;
const int MODE_MULTIPLY            = 2;
const int MODE_SCREEN              = 3;
const int MODE_LIGHTEN             = 4;
const int MODE_DARKEN              = 5;
const int MODE_DIFFERENCE          = 6;
const int MODE_OVERLAY             = 7;
const int MODE_ATOP                = 8;
const int MODE_AVERAGE             = 9;
const int MODE_BRIGHTEST           = 10;
const int MODE_BURN_COLOR          = 11;
const int MODE_BURN_LINEAR         = 12;
const int MODE_CHROMA_DIFFERENCE   = 13;
const int MODE_COLOR_BLEND         = 14;
const int MODE_DARKER_COLOR        = 15;
const int MODE_DIMMEST             = 16;
const int MODE_DIVIDE              = 17;
const int MODE_DODGE               = 18;
const int MODE_EXCLUDE             = 19;
const int MODE_FREEZE              = 20;
const int MODE_GLOW                = 21;
const int MODE_HARD_LIGHT          = 22;
const int MODE_HARD_MIX            = 23;
const int MODE_HEAT                = 24;
const int MODE_HUE_BLEND           = 25;
const int MODE_INSIDE              = 26;
const int MODE_INSIDE_LUMINANCE    = 27;
const int MODE_INVERSE             = 28;
const int MODE_LIGHTER_COLOR       = 29;
const int MODE_LUMINANCE_DIFFERENCE = 30;
const int MODE_MAXIMUM             = 31;
const int MODE_MINIMUM             = 32;
const int MODE_NEGATE              = 33;
const int MODE_OUTSIDE             = 34;
const int MODE_OUTSIDE_LUMINANCE   = 35;
const int MODE_OVER                = 36;
const int MODE_PINLIGHT            = 37;
const int MODE_REFLECT             = 38;
const int MODE_SOFT_LIGHT          = 39;
const int MODE_LINEAR_LIGHT        = 40;
const int MODE_STENCIL_LUMINANCE   = 41;
const int MODE_SUBTRACT            = 42;
const int MODE_SUBTRACTIVE         = 43;
const int MODE_UNDER               = 44;
const int MODE_VIVID_LIGHT         = 45;
const int MODE_XOR                 = 46;
const int MODE_Y_FILM              = 47;
const int MODE_Z_FILM              = 48;
const int MODE_DIFFERENCE_VIVID    = 49;
const int MODE_DIFFERENCE_RGB      = 50;
const int MATTE_NONE               = 0;
const int MATTE_LUMA               = 1;
const int MATTE_ALPHA              = 2;
const int MATTE_KNOCKOUT           = 3;

const vec3 kBg = vec3(0.047, 0.047, 0.047);
const vec3 kLum = vec3(0.2126, 0.7152, 0.0722);
const float kEps = 1e-5;

vec3 clampRgb(vec3 c) { return clamp(c, vec3(0.0), vec3(1.0)); }

float lum(vec3 c) { return dot(c, kLum); }

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

vec3 hsl2rgb(vec3 hsl)
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

vec3 rgb2hsl(vec3 c)
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

vec3 blendNormal(vec3 a, vec3 b, float alphaB)
{
    return b * alphaB + a * (1.0 - alphaB);
}

vec3 blendColorBlend(vec3 a, vec3 b)
{
    vec3 ha = rgb2hsl(a);
    vec3 hb = rgb2hsl(b);
    return hsl2rgb(vec3(hb.x, hb.y, ha.z));
}

vec3 blendHueBlend(vec3 a, vec3 b)
{
    vec3 ha = rgb2hsl(a);
    vec3 hb = rgb2hsl(b);
    return hsl2rgb(vec3(hb.x, ha.y, ha.z));
}

vec3 blendOverlay(vec3 a, vec3 b)
{
    vec3 low = 2.0 * a * b;
    vec3 hi  = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
    return mix(low, hi, step(0.5, a));
}

vec3 blendSoftLight(vec3 a, vec3 b)
{
    vec3 low = a - (1.0 - 2.0 * b) * a * (1.0 - a);
    vec3 s = sqrt(clamp(a, vec3(0.0), vec3(1.0)));
    vec3 high = a + (2.0 * b - 1.0) * (s - a);
    return mix(low, high, step(vec3(0.5), b));
}

vec3 blendColorBurn(vec3 a, vec3 b)
{
    return clampRgb(1.0 - (1.0 - a) / max(b, vec3(kEps)));
}

vec3 blendColorDodge(vec3 a, vec3 b)
{
    return clampRgb(a / max(1.0 - b, vec3(kEps)));
}

vec3 blendVividLight(vec3 a, vec3 b)
{
    return mix(blendColorBurn(a, 2.0 * b), blendColorDodge(a, 2.0 * b - 1.0), step(vec3(0.5), b));
}

vec3 blendHardLight(vec3 a, vec3 b)
{
    return blendOverlay(b, a);
}

vec3 blendPinlight(vec3 a, vec3 b)
{
    return mix(min(a, 2.0 * b), max(a, 2.0 * b - 1.0), step(vec3(0.5), b));
}

vec3 blendLinearLight(vec3 a, vec3 b)
{
    return clampRgb(a + 2.0 * b - 1.0);
}

vec3 blendHardMix(vec3 a, vec3 b)
{
    // Photoshop-like hard mix: thresholded vivid light result.
    return step(vec3(0.5), blendVividLight(a, b));
}

vec3 blendReflect(vec3 a, vec3 b)
{
    // Reference: Photoshop math (Reflect).
    return clampRgb((b * b) / max(1.0 - a, vec3(kEps)));
}

vec3 blendGlow(vec3 a, vec3 b)
{
    // Glow is the swapped Reflect variant.
    return clampRgb((a * a) / max(1.0 - b, vec3(kEps)));
}

vec3 blendFreeze(vec3 a, vec3 b)
{
    // Reference formula: 1 - (1-a)^2 / b
    return clampRgb(1.0 - ((1.0 - a) * (1.0 - a)) / max(b, vec3(kEps)));
}

vec3 blendHeat(vec3 a, vec3 b)
{
    // Reference formula: 1 - (1-b)^2 / a
    return clampRgb(1.0 - ((1.0 - b) * (1.0 - b)) / max(a, vec3(kEps)));
}

vec3 pickLighter(vec3 a, vec3 b)
{
    return lum(a) > lum(b) ? a : b;
}

vec3 pickDarker(vec3 a, vec3 b)
{
    return lum(a) < lum(b) ? a : b;
}

vec3 blendAtopApprox(vec3 dst, vec3 src)
{
    float da = lum(dst);
    float sa = max(max(src.r, src.g), src.b);
    return clampRgb(dst * sa + src * (1.0 - da));
}

vec3 blendOpaque(int mode, vec3 a, vec3 b)
{
    switch (mode) {
    case MODE_ADD:               return clampRgb(a + b);
    case MODE_MULTIPLY:          return a * b;
    case MODE_SCREEN:            return 1.0 - (1.0 - a) * (1.0 - b);
    case MODE_LIGHTEN:           return max(a, b);
    case MODE_DARKEN:            return min(a, b);
    case MODE_DIFFERENCE:        return abs(a - b);
    case MODE_DIFFERENCE_VIVID: {
        vec3 d = abs(a - b);
        vec3 h = rgb2hsl(clampRgb(d));
        h.x = fract(h.x + 0.08);      // subtle rainbow drift
        h.y = clamp(h.y * 1.8, 0.0, 1.0);
        vec3 vivid = hsl2rgb(h) * 1.15;
        return clampRgb(vivid);
    }
    case MODE_DIFFERENCE_RGB:    return abs(a - b.bgr);
    case MODE_OVERLAY:           return blendOverlay(a, b);
    case MODE_ATOP:              return blendAtopApprox(a, b);
    case MODE_AVERAGE:           return (a + b) * 0.5;
    case MODE_BRIGHTEST:         return pickLighter(a, b);
    case MODE_BURN_COLOR:        return blendColorBurn(a, b);
    case MODE_BURN_LINEAR:       return clampRgb(a + b - 1.0);
    case MODE_CHROMA_DIFFERENCE: {
        float d = distance(a, b);
        return vec3(d);
    }
    case MODE_COLOR_BLEND:       return blendColorBlend(a, b);
    case MODE_DARKER_COLOR:      return pickDarker(a, b);
    case MODE_DIMMEST:           return pickDarker(a, b);
    case MODE_DIVIDE:            return clampRgb(a / max(b, vec3(kEps)));
    case MODE_DODGE:             return blendColorDodge(a, b);
    case MODE_EXCLUDE:           return clampRgb(a + b - 2.0 * a * b);
    case MODE_FREEZE:            return blendFreeze(a, b);
    case MODE_GLOW:              return blendGlow(a, b);
    case MODE_HARD_LIGHT:        return blendHardLight(a, b);
    case MODE_HARD_MIX:          return blendHardMix(a, b);
    case MODE_HEAT:              return blendHeat(a, b);
    case MODE_HUE_BLEND:         return blendHueBlend(a, b);
    case MODE_INSIDE:            return a * max(max(b.r, b.g), b.b);
    case MODE_INSIDE_LUMINANCE:  return a * lum(b);
    case MODE_INVERSE:           return 1.0 - b;
    case MODE_LIGHTER_COLOR:     return pickLighter(a, b);
    case MODE_LUMINANCE_DIFFERENCE: return vec3(abs(lum(a) - lum(b)));
    case MODE_MAXIMUM:           return max(a, b);
    case MODE_MINIMUM:           return min(a, b);
    case MODE_NEGATE:            return 1.0 - abs(a - b);
    case MODE_OUTSIDE:           return a * (1.0 - max(max(b.r, b.g), b.b));
    case MODE_OUTSIDE_LUMINANCE: return a * (1.0 - lum(b));
    case MODE_PINLIGHT:          return blendPinlight(a, b);
    case MODE_REFLECT:           return blendReflect(a, b);
    case MODE_SOFT_LIGHT:        return blendSoftLight(a, b);
    case MODE_LINEAR_LIGHT:      return blendLinearLight(a, b);
    case MODE_STENCIL_LUMINANCE: return a * smoothstep(0.35, 0.65, lum(b));
    case MODE_SUBTRACT:          return clampRgb(a - b);
    case MODE_SUBTRACTIVE:       return clampRgb(a + b - 1.0);
    case MODE_VIVID_LIGHT:       return blendVividLight(a, b);
    case MODE_XOR:               return clampRgb(a + b - 2.0 * a * b);
    case MODE_Y_FILM: {
        float yb = lum(b);
        vec3 yOnly = vec3(yb);
        return mix(a, yOnly, 0.5);
    }
    case MODE_Z_FILM: {
        float ya = lum(a);
        return mix(b, vec3(ya), 0.5);
    }
    default:
        return b;
    }
}

vec3 compositeLayer(int mode, vec3 dst, vec3 src, float alpha)
{
    if (mode == MODE_NORMAL || mode == MODE_OVER || mode == MODE_UNDER) {
        return blendNormal(dst, src, alpha);
    }
    vec3 blended = blendOpaque(mode, dst, src);
    return mix(dst, blended, alpha);
}

vec4 sampleLayer(int i)
{
    switch (i) {
    case 0:  return texture(u_tex0, v_uv);
    case 1:  return texture(u_tex1, v_uv);
    case 2:  return texture(u_tex2, v_uv);
    case 3:  return texture(u_tex3, v_uv);
    case 4:  return texture(u_tex4, v_uv);
    case 5:  return texture(u_tex5, v_uv);
    case 6:  return texture(u_tex6, v_uv);
    case 7:  return texture(u_tex7, v_uv);
    case 8:  return texture(u_tex8, v_uv);
    case 9:  return texture(u_tex9, v_uv);
    case 10: return texture(u_tex10, v_uv);
    case 11: return texture(u_tex11, v_uv);
    default: return vec4(0.0);
    }
}

void main()
{
    vec3 dst = kBg;
    bool hasBase = false;

    int maxLayer = int(ubuf.mixerCfg.x + 0.5);
    if (maxLayer <= 0) maxLayer = 12;
    int cfgLayer = int(ubuf.mixerCfg.y + 0.5);
    const bool feedbackActive = ubuf.mixerCfg.z > 0.5;
    const int feedbackLayer = feedbackActive ? cfgLayer : -1;

    for (int i = 0; i < 12; i++) {
        if (feedbackActive) {
            if (i >= maxLayer) continue;
        } else {
            if (i < cfgLayer || i >= maxLayer) continue;
        }
        vec4 lp = ubuf.layers[i];
        if (lp.z < 0.5) {
            continue;
        }

        vec4 c = sampleLayer(i);
        float op = clamp(lp.x, 0.0, 1.0);
        // Keep source color unscaled for blend equations (e.g. Difference = |dst-src|).
        // Opacity belongs in alpha compositing weight, not baked into blend input.
        vec3 src = c.rgb;
        float alpha = clamp(c.a * op, 0.0, 1.0);
        int mode = int(lp.y + 0.5);
        int matteRole = int(lp.w + 0.5);

        // Feedback layer: fade over the normal mix below; key from layers above
        // uses per-layer alpha/coverage so keyed holes stay open for feedback.
        const bool isFeedbackLayer = feedbackActive && i == feedbackLayer;
        if (isFeedbackLayer) {
            vec3 under = dst;
            float vis = op;
            // mixerCfg.w: 0 = no key mask, 1 = BelowOnly hole mask, 2 = unified (full trail)
            if (ubuf.mixerCfg.w > 0.5 && ubuf.mixerCfg.w < 1.5) {
                float upperCov = 0.0;
                for (int j = feedbackLayer + 1; j < 12; j++) {
                    vec4 aboveLp = ubuf.layers[j];
                    if (aboveLp.z < 0.5) {
                        continue;
                    }
                    vec4 aboveSample = sampleLayer(j);
                    float aboveOp = clamp(aboveLp.x, 0.0, 1.0);
                    float alphaCov = clamp(aboveSample.a * aboveOp, 0.0, 1.0);
                    float lumaCov = clamp(length(aboveSample.rgb - kBg) * 1.8, 0.0, 1.0) * aboveOp;
                    upperCov = max(upperCov, max(alphaCov, lumaCov));
                }
                vis *= (1.0 - upperCov);
            }
            if (vis <= 1e-4) {
                dst = under;
            } else {
                dst = blendNormal(under, src, vis);
            }
            hasBase = true;
            continue;
        }

        if (matteRole == MATTE_LUMA) {
            float m = mix(1.0, lum(src), alpha);
            dst *= m;
            hasBase = true;
            continue;
        }
        if (matteRole == MATTE_ALPHA) {
            dst *= (1.0 - alpha);
            hasBase = true;
            continue;
        }
        if (matteRole == MATTE_KNOCKOUT) {
            dst *= (1.0 - alpha);
            hasBase = true;
            continue;
        }

        if (!hasBase) {
            dst = compositeLayer(mode, kBg, src, alpha);
            hasBase = true;
        } else {
            dst = compositeLayer(mode, dst, src, alpha);
        }
    }

    if (!hasBase) {
        fragColor = vec4(kBg, 1.0);
    } else {
        fragColor = vec4(dst, 1.0);
    }
}
