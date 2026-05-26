#version 440

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

layout(std140, binding = 0) uniform Block {
    // xy = post-projection scale (for aspect letterboxing), zw = time/aspect for fragment effects.
    vec4 scaleOffset;
    // x = optional vertex rotation (radians); yzw used by fragment effects (e.g. key weights, effectId).
    vec4 rotation;
  // Fragment effect parameters; VS does not read these.
    vec4 params;
    vec4 params2;
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
