#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

// Coverage shader uses the same bindings/UBO layout as `mixer.frag`
// so it can reuse the same per-layer texture atlas/SRB strategy.
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
layout(binding = 13) uniform sampler2D u_tex12;

// Not used by this shader, but declared to keep binding layout aligned with `mixer.frag`.
layout(binding = 14) uniform sampler2D u_under;
layout(binding = 15) uniform sampler2D u_aboveKey;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 layers[14];
    vec4 picUvA[14];
    vec4 picColor[14];
    vec4 mixerCfg; // x=maxLayerExclusive, y=minLayerInclusive, z/w unused
} ubuf;

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

vec2 transformLayerUv(vec2 uv, float zoom, float rotRad)
{
    vec2 p = uv - vec2(0.5);
    float scale = 1.0 - zoom * 0.5;
    p /= max(scale, 0.01);
    float cs = cos(rotRad);
    float sn = sin(rotRad);
    p = vec2(cs * p.x - sn * p.y, sn * p.x + cs * p.y);
    return p + vec2(0.5);
}

vec4 sampleLayerRaw(int texIndex, vec2 uv)
{
    switch (texIndex) {
    case 0:  return texture(u_tex0, uv);
    case 1:  return texture(u_tex1, uv);
    case 2:  return texture(u_tex2, uv);
    case 3:  return texture(u_tex3, uv);
    case 4:  return texture(u_tex4, uv);
    case 5:  return texture(u_tex5, uv);
    case 6:  return texture(u_tex6, uv);
    case 7:  return texture(u_tex7, uv);
    case 8:  return texture(u_tex8, uv);
    case 9:  return texture(u_tex9, uv);
    case 10: return texture(u_tex10, uv);
    case 11: return texture(u_tex11, uv);
    case 12: return texture(u_tex12, uv);
    default: return vec4(0.0);
    }
}

vec4 sampleLayer(int i)
{
    if (i == 0) {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
    vec4 pa = ubuf.picUvA[i];
    vec4 pc = ubuf.picColor[i];
    vec2 uv = transformLayerUv(v_uv, pa.x, pa.y);
    const int wrapMode = int(pc.w + 0.5);
    const bool oob = uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0;
    uv = applyWrap(uv, min(wrapMode, 3));
    vec4 c = sampleLayerRaw(i - 1, uv);
    if (wrapMode == 4 && oob) {
        return vec4(0.0);
    }
    return c;
}

void main()
{
    int maxLayer = int(ubuf.mixerCfg.x + 0.5);
    if (maxLayer <= 0) maxLayer = 14;
    int minLayer = int(ubuf.mixerCfg.y + 0.5);
    if (minLayer < 0) minLayer = 0;

    float upperCov = 0.0;
    for (int j = 0; j < 14; j++) {
        if (j < minLayer || j >= maxLayer) continue;

        vec4 aboveLp = ubuf.layers[j];
        if (aboveLp.z < 0.5) {
            continue;
        }

        vec4 aboveSample = sampleLayer(j);
        float aboveOp = clamp(aboveLp.x, 0.0, 1.0);
        // Key filters keep source RGB and put the key in alpha — coverage must
        // follow alpha only. Luma-of-RGB would punch holes over the whole frame
        // and hide the feedback trail under keyed layers.
        float alphaCov = clamp(aboveSample.a * aboveOp, 0.0, 1.0);
        upperCov = max(upperCov, alphaCov);
    }

    fragColor = vec4(vec3(upperCov), 1.0);
}
