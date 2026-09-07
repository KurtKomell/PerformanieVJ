#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_live;
layout(binding = 2) uniform sampler2D u_history;

layout(std140, binding = 0) uniform Block {
    vec4 fbA; // passMode, pathSaturation, pathBrightness, pathContrast
    vec4 fbB; // pathHueShift, pathGamma, rotationDeg, zoom
    vec4 fbC; // translateX, translateY, retention, wrapMode
    vec4 fbD; // inBrightness, inContrast, inSaturation, inHueShift
    vec4 fbE; // inGamma, blendMode, stageWidthPx, stageHeightPx
} ubuf;

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

const vec3 kLum = vec3(0.2126, 0.7152, 0.0722);

vec3 applyGrading(vec3 c, float brightness, float contrast, float saturation, float hueShift, float gamma)
{
    c = (c - 0.5) * contrast + 0.5 + brightness;
    c = clamp(c, vec3(0.0), vec3(1.0));
    float luma = dot(c, kLum);
    if (abs(hueShift) > 1e-5) {
        vec3 hsl = rgb2hsl(c);
        if (hsl.y > 0.02) {
            hsl.x = fract(hsl.x + hueShift);
            c = hsl2rgb(hsl);
        }
    }
    c = mix(vec3(luma), c, clamp(saturation, 0.0, 2.0));
    c = pow(clamp(c, vec3(0.0), vec3(1.0)), vec3(1.0 / max(gamma, 0.01)));
    return c;
}

/// Classic feedback pixel shift: scale + rotate about 0.5, then UV translate.
vec2 transformUv(vec2 uv)
{
    float zoom = ubuf.fbB.w;
    float rotDeg = ubuf.fbB.z;
    float tx = ubuf.fbC.x;
    float ty = ubuf.fbC.y;

    vec2 p = uv - vec2(0.5);
    float scale = 1.0 + zoom * 0.5;
    p /= max(scale, 0.01);

    float ang = rotDeg * 3.14159265358979323846 / 180.0;
    float cs = cos(ang);
    float sn = sin(ang);
    p = vec2(cs * p.x - sn * p.y, sn * p.x + cs * p.y);

    return p + vec2(0.5) + vec2(tx, ty);
}

vec2 applyWrap(vec2 uv, int mode)
{
    if (mode == 1) {
        return fract(uv);
    }
    if (mode == 2) {
        vec2 t = fract(uv * 0.5) * 2.0;
        return 1.0 - abs(t - 1.0);
    }
    if (mode == 3) {
        vec2 c = clamp(uv, 0.0, 2.0);
        return 1.0 - abs(c - 1.0);
    }
    return clamp(uv, 0.0, 1.0);
}

/// Loop blend: live vs retained history (FeedbackBlendMode).
/// histScaled = gradedHistory * retention; graded = unscaled graded history.
vec3 blendFeedback(vec3 live, vec3 histScaled, vec3 graded, float retention, int mode)
{
    if (mode == 1) {
        // Mix: retention is lerp weight (0 = live only, 1 = history only).
        return mix(live, graded, clamp(retention, 0.0, 1.0));
    }
    if (mode == 2) {
        return 1.0 - (1.0 - live) * (1.0 - histScaled); // screen
    }
    if (mode == 3) {
        return max(live, histScaled); // lighten
    }
    if (mode == 4) {
        return live * mix(vec3(1.0), graded, clamp(retention, 0.0, 1.0)); // multiply
    }
    if (mode == 5) {
        return abs(live - histScaled); // difference
    }
    // Add (default)
    return live + histScaled;
}

void main()
{
    // passMode 0: input-grade live only (prepare for filters)
    // passMode 1: blend(live, retention, grade(transform(history)))
    // passMode 2: fade-only (no live)
    const int passMode = int(ubuf.fbA.x + 0.5);
    const int wrapMode = int(ubuf.fbC.w + 0.5);
    const int blendMode = int(ubuf.fbE.y + 0.5);
    const float retention = clamp(ubuf.fbC.z, 0.0, 1.0);

    if (passMode == 0) {
        vec3 live = texture(u_live, v_uv).rgb;
        live = applyGrading(live,
                            ubuf.fbD.x,
                            ubuf.fbD.y,
                            ubuf.fbD.z,
                            ubuf.fbD.w,
                            max(ubuf.fbE.x, 0.01));
        fragColor = vec4(clamp(live, 0.0, 1.0), 1.0);
        return;
    }

    vec3 live = texture(u_live, v_uv).rgb;

    vec2 huv = transformUv(v_uv);
    const bool oob = huv.x < 0.0 || huv.x > 1.0 || huv.y < 0.0 || huv.y > 1.0;
    vec3 graded = vec3(0.0);
    vec3 histScaled = vec3(0.0);
    if (!(wrapMode == 4 && oob)) {
        graded = texture(u_history, applyWrap(huv, wrapMode)).rgb;
        graded = applyGrading(graded,
                            ubuf.fbA.z,
                            ubuf.fbA.w,
                            ubuf.fbA.y,
                            ubuf.fbB.x,
                            max(ubuf.fbB.y, 0.01));
        histScaled = graded * retention;
    }

    if (passMode == 2) {
        fragColor = vec4(clamp(histScaled, 0.0, 1.0), 1.0);
        return;
    }
    fragColor = vec4(clamp(blendFeedback(live, histScaled, graded, retention, blendMode), 0.0, 1.0), 1.0);
}
