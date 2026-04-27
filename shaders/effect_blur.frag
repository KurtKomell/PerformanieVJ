#version 440

// Single-pass separable-ish blur: 9-tap Gaussian in 2D (3x3) with optional extra ring.
// UBO matches ShaderLibrary.h EffectBlurUbo / textured_quad.vert layout prefix.

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params; // x = blur radius in UV space (typ. 0.001–0.02), yzw = unused
} ubuf;

void main()
{
    float r = max(ubuf.params.x, 0.0);
    // 3x3 Gaussian (sigma ~ 1) — weights sum ~ 1
    const float w00 = 0.0625; const float w01 = 0.125; const float w02 = 0.0625;
    const float w10 = 0.125;  const float w11 = 0.25;  const float w12 = 0.125;
    const float w20 = 0.0625; const float w21 = 0.125; const float w22 = 0.0625;

    vec2 o = vec2(r, r);
    vec3 c =
        texture(u_tex, v_uv + vec2(-o.x, -o.y)).rgb * w00 +
        texture(u_tex, v_uv + vec2( 0.0, -o.y)).rgb * w01 +
        texture(u_tex, v_uv + vec2( o.x, -o.y)).rgb * w02 +
        texture(u_tex, v_uv + vec2(-o.x,  0.0)).rgb * w10 +
        texture(u_tex, v_uv + vec2( 0.0,  0.0)).rgb * w11 +
        texture(u_tex, v_uv + vec2( o.x,  0.0)).rgb * w12 +
        texture(u_tex, v_uv + vec2(-o.x,  o.y)).rgb * w20 +
        texture(u_tex, v_uv + vec2( 0.0,  o.y)).rgb * w21 +
        texture(u_tex, v_uv + vec2( o.x,  o.y)).rgb * w22;

    fragColor = vec4(c, texture(u_tex, v_uv).a);
}
