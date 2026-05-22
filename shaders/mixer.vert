#version 440

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;

// std140 block shared with mixer.frag. scaleOffset.xy = aspect scale,
// scaleOffset.z = elapsed time (seconds), .w reserved.
layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 layers[12];
    vec4 picUvA[12];
    vec4 picColor[12];
    vec4 mixerCfg; // reserved mixer configuration
} ubuf;

void main()
{
    vec2 p = a_position * ubuf.scaleOffset.xy;
    gl_Position = vec4(p, 0.0, 1.0);
    v_uv = a_uv;
}
