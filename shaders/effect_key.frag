#version 440

// Chroma / luma keying. UBO matches textured_quad.vert layout:
// scaleOffset: xy quad scale, z = time, w = aspect (unused here)
// scaleOffset.z + rotation.yz = per-cell RGB key channel weights (from inspector)
// rotation.w = 0 chroma_key, 1 luma_key
// params: x = mode enum index, y = hue (chroma) or brightness center (luma),
//         z = threshold, w = softness

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
} ubuf;

float hue2rgb(float p, float q, float t)
{
    float x = t;
    if (x < 0.0) {
        x += 1.0;
    }
    if (x > 1.0) {
        x -= 1.0;
    }
    if (x < 1.0 / 6.0) {
        return p + (q - p) * 6.0 * x;
    }
    if (x < 0.5) {
        return q;
    }
    if (x < 2.0 / 3.0) {
        return p + (q - p) * (2.0 / 3.0 - x) * 6.0;
    }
    return p;
}

vec3 hsl2rgb_f(vec3 hsl)
{
    float h = fract(hsl.x);
    float s = hsl.y;
    float l = hsl.z;
    if (s < 1e-5) {
        return vec3(l);
    }
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
        if (maxc == c.r) {
            h = (c.g - c.b) / delta + (c.g < c.b ? 6.0 : 0.0);
        } else if (maxc == c.g) {
            h = (c.b - c.r) / delta + 2.0;
        } else {
            h = (c.r - c.g) / delta + 4.0;
        }
        h /= 6.0;
    }
    return vec3(h, s, l);
}

void main()
{
    vec4 tex = texture(u_tex, v_uv);
    vec3 c = clamp(tex.rgb, vec3(0.0), vec3(1.0));
    float baseA = tex.a;

    vec3 wRaw = max(vec3(ubuf.scaleOffset.z, ubuf.rotation.y, ubuf.rotation.z), vec3(0.001));
    float wsum = wRaw.x + wRaw.y + wRaw.z;
    vec3 wN = wRaw / wsum;

    float mode = ubuf.params.x;
    float pKey = clamp(ubuf.params.y, 0.0, 1.0);
    float pTh = max(ubuf.params.z, 0.002);
    float pSoft = max(ubuf.params.w, 0.0005);

    int mi = int(mode + 0.01);
    float keyedAlpha = 1.0;

    if (ubuf.rotation.w < 0.5) {
        // --- Chroma key family ---
        vec3 keyRgb = hsl2rgb_f(vec3(fract(pKey), 1.0, 0.5));
        float metric = 0.0;
        if (mi <= 1) {
            vec3 hsl = rgb2hsl(c);
            float dh = abs(hsl.x - fract(pKey));
            dh = min(dh, 1.0 - dh) * 2.0;
            float satBoost = mix(0.15, 1.0, hsl.y);
            metric = dh * satBoost;
        } else if (mi <= 3) {
            vec3 d = (c - keyRgb) * wRaw;
            metric = length(d);
        } else {
            vec3 d = abs((c - keyRgb) * wRaw);
            metric = max(max(d.x, d.y), d.z);
        }
        keyedAlpha = smoothstep(pTh - pSoft, pTh + pSoft, metric);
        if ((mi % 2) != 0) {
            keyedAlpha = 1.0 - keyedAlpha;
        }
    } else {
        // --- Luma / channel family ---
        float v = 0.0;
        if (mi == 0) {
            v = dot(c, wN);
        } else if (mi == 1) {
            v = 1.0 - dot(c, wN);
        } else if (mi == 2) {
            v = dot(c, vec3(0.2126, 0.7152, 0.0722));
        } else if (mi == 3) {
            v = 1.0 - dot(c, vec3(0.2126, 0.7152, 0.0722));
        } else if (mi == 4) {
            v = c.r;
        } else if (mi == 5) {
            v = c.g;
        } else if (mi == 6) {
            v = c.b;
        } else if (mi == 7) {
            v = max(max(c.r, c.g), c.b);
        } else if (mi == 8) {
            v = min(min(c.r, c.g), c.b);
        } else {
            v = 1.0 - max(max(c.r, c.g), c.b);
        }
        float d = abs(v - pKey);
        keyedAlpha = smoothstep(pTh + pSoft, pTh - pSoft, d);
    }

    fragColor = vec4(c, baseA * clamp(keyedAlpha, 0.0, 1.0));
}
