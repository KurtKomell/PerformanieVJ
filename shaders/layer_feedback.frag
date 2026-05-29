#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_below;
layout(binding = 2) uniform sampler2D u_history;

layout(std140, binding = 0) uniform Block {
    vec4 fbA; // loopRetention, saturation, brightness, contrast (history)
    vec4 fbB; // hueShift, gamma, rotationDeg, zoom (history hue/gamma + transform)
    vec4 fbC; // centerX, centerY, liveInject, wrapMode
    vec4 fbD; // inBrightness, inContrast, inSaturation, inHueShift (input grade)
    vec4 fbE; // inGamma, layerOpacity, 0, 0
} ubuf;

vec2 transformUv(vec2 uv)
{
    float zoom = ubuf.fbB.w;
    float rotDeg = ubuf.fbB.z;
    float cx = ubuf.fbC.x;
    float cy = ubuf.fbC.y;

    vec2 p = uv - vec2(cx, cy);
    float scale = 1.0 + zoom * 0.5;
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

vec3 sampleWarpedHistory(sampler2D historyTex, vec2 screenUv, int wrapMode)
{
    vec2 histUv = transformUv(screenUv);
    const bool histOob = histUv.x < 0.0 || histUv.x > 1.0 || histUv.y < 0.0 || histUv.y > 1.0;
    if (wrapMode == 4) {
        if (histOob) {
            return vec3(0.0);
        }
        return texture(historyTex, histUv).rgb;
    }
    histUv = applyWrap(histUv, wrapMode);
    return texture(historyTex, histUv).rgb;
}

void main()
{
    const int wrapMode = int(ubuf.fbC.w + 0.5);

    // Ungraded inject; input color is for live video only (mixer), not baked into the ring.
    vec3 below = texture(u_below, v_uv).rgb;

    // Ring stores ungraded accumulation; history color is applied on read (hist display pass).
    vec3 historyRaw = sampleWarpedHistory(u_history, v_uv, wrapMode);

    float layerOpacity = clamp(ubuf.fbE.y, 0.0, 1.0);
    if (layerOpacity <= 1e-4) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Ping-pong loop: live inject + warped history. Lit history pixels keep full retention
    // (infinite loop while signal is present); only empty areas fade via loopRetention.
    float retention = clamp(ubuf.fbA.x, 0.0, 1.0);
    float inject = clamp(ubuf.fbC.z, 0.0, 1.0) * (1.0 - layerOpacity);
    float signalHold = smoothstep(0.012, 0.04, length(historyRaw));
    float effectiveRetention = mix(retention, 1.0, signalHold);
    vec3 outRgb = clamp(below * inject + historyRaw * effectiveRetention, 0.0, 1.0);
    fragColor = vec4(outRgb, 1.0);
}
