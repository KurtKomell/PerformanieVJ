#version 440

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

layout(std140, binding = 0) uniform Block {
    // xy = post-projection scale (for aspect letterboxing), zw = unused in VS.
    // CPU filter path stores time/ratio in .zw for *fragment* effects (e.g. distort) —
    // do not add .zw in vertex position (would shove the quad out of the viewport).
    vec4 scaleOffset;
    // x = rotation angle (radians), yzw = unused.
    vec4 rotation;
    // Match effect_*.frag / ShaderLibrary::EffectQuadUbo (48 bytes total).
    vec4 params;
} ubuf;

void main()
{
    vec2 p = a_position * ubuf.scaleOffset.xy;
    float c = cos(ubuf.rotation.x);
    float s = sin(ubuf.rotation.x);
    p = mat2(c, -s, s, c) * p;
    gl_Position = vec4(p, 0.0, 1.0);
    v_uv = a_uv;
}
