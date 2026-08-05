#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_hist;      // RGBA: trail/history (RGB used)
layout(binding = 2) uniform sampler2D u_coverage;  // RGB: coverage mask (R used)

layout(std140, binding = 0) uniform Block {
    float applyMask; // 0 = unified trail, 1 = apply hole mask using (1 - coverage)
    vec3 _pad;
} ubuf;

void main()
{
    vec4 hist = texture(u_hist, v_uv);
    float alpha = 1.0;
    if (ubuf.applyMask > 0.5) {
        float cov = texture(u_coverage, v_uv).r;
        alpha = 1.0 - cov;
    }
    // mixer.frag will use `alpha = c.a * layerOpacity`, so we output the mask only.
    fragColor = vec4(hist.rgb, alpha);
}

