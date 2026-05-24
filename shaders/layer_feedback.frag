#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_below;
layout(binding = 2) uniform sampler2D u_history;
layout(binding = 3) uniform sampler2D u_above;

layout(std140, binding = 0) uniform Block {
    vec4 fbA; // loopRetention, saturation, brightness, contrast
    vec4 fbB; // hueShift, gamma, rotationDeg, zoom
    vec4 fbC; // centerX, centerY, liveInject, wrapMode
} ubuf;

const vec3 kBg = vec3(0.0, 0.0, 0.0);

const vec3 kLum = vec3(0.2126, 0.7152, 0.0722);

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

vec3 applyGrading(vec3 c)
{
    float brightness = ubuf.fbA.z;
    float contrast   = ubuf.fbA.w;
    float saturation = ubuf.fbA.y;
    float hueShift   = ubuf.fbB.x;
    float gamma      = max(ubuf.fbB.y, 0.01);

    c = (c - 0.5) * contrast + 0.5 + brightness;
    vec3 hsl = rgb2hsl(clamp(c, vec3(0.0), vec3(1.0)));
    hsl.x = fract(hsl.x + hueShift);
    hsl.y = clamp(hsl.y * saturation, 0.0, 1.0);
    c = hsl2rgb(hsl);
    c = pow(clamp(c, vec3(0.0), vec3(1.0)), vec3(1.0 / gamma));
    return c;
}

vec2 transformUv(vec2 uv)
{
    float zoom = ubuf.fbB.w;
    float rotDeg = ubuf.fbB.z;
    float cx = ubuf.fbC.x;
    float cy = ubuf.fbC.y;

    vec2 p = uv - vec2(cx, cy);
    float scale = 1.0 - zoom * 0.5;
    p /= max(scale, 0.01);

    float ang = rotDeg * 3.14159265358979323846 / 180.0;
    float cs = cos(ang);
    float sn = sin(ang);
    p = vec2(cs * p.x - sn * p.y, sn * p.x + cs * p.y);

    return p + vec2(cx, cy);
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

void main()
{
    const int wrapMode = int(ubuf.fbC.w + 0.5);

    vec3 below = texture(u_below, v_uv).rgb;
    vec2 histUv = transformUv(v_uv);
    const bool histOob = histUv.x < 0.0 || histUv.x > 1.0 || histUv.y < 0.0 || histUv.y > 1.0;
    const int wrapApply = min(wrapMode, 3);
    histUv = applyWrap(histUv, wrapApply);

    vec3 history = vec3(0.0);
    if (wrapMode != 4 || !histOob) {
        history = texture(u_history, histUv).rgb;
    }
    history = applyGrading(history);

    // Ping-pong loop: additive retention (warped history) + live inject (fresh source).
    float retention = clamp(ubuf.fbA.x, 0.0, 1.0);
    float inject    = clamp(ubuf.fbC.z, 0.0, 1.0);
    vec3 outRgb = clamp(below * inject + history * retention, 0.0, 1.0);
    fragColor = vec4(outRgb, 1.0);
}
