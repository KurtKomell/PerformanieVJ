#version 440

// Brightness / contrast / saturation / hue shift (radians).
// params: x=brightness add [-1,1], y=contrast mult (1=neutral), z=saturation (1=neutral),
//         w=hue shift radians

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_tex;

layout(std140, binding = 0) uniform Block {
    vec4 scaleOffset;
    vec4 rotation;
    vec4 params;
} ubuf;

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

void main()
{
    vec4 tex = texture(u_tex, v_uv);
    vec3 rgb = tex.rgb;

    rgb += ubuf.params.x;
    float contrast = max(ubuf.params.y, 0.001);
    rgb = (rgb - 0.5) * contrast + 0.5;

    float lum = dot(rgb, kLum);
    rgb = mix(vec3(lum), rgb, max(ubuf.params.z, 0.0));

    vec3 hsl = rgb2hsl(clamp(rgb, vec3(0.0), vec3(1.0)));
    hsl.x = fract(hsl.x + ubuf.params.w / 6.28318530718);
    rgb = hsl2rgb(hsl);

    fragColor = vec4(clamp(rgb, vec3(0.0), vec3(1.0)), tex.a);
}
