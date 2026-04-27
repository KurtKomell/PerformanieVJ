#version 440

// Fullscreen quad for A/B transitions. UBO: letterbox scale + transition params.

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 transition; // x=progress [0,1], y=type index, zw = slide direction
} ubuf;

void main()
{
    vec2 p = a_position * ubuf.scaleOffset.xy + ubuf.scaleOffset.zw;
    gl_Position = vec4(p, 0.0, 1.0);
    v_uv = a_uv;
}
