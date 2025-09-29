#version 460 core

in vec2 vUV;
flat in int vType;

uniform sampler2DArray flowerTex;

// 4x4 ordered Bayer dither thresholds (0..1)
float bayer4x4(int x, int y)
{
    const float m[16] = float[16](
        0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
        12.0/16.0, 4.0/16.0, 14.0/16.0,  6.0/16.0,
        3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
        15.0/16.0, 7.0/16.0, 13.0/16.0,  5.0/16.0);
    int i = (y & 3) * 4 + (x & 3);
    return m[i];
}

void main()
{
    float a = texture(flowerTex, vec3(vUV, float(vType))).a;
    // Ordered dithering of alpha to approximate fractional coverage in shadow map
    float threshold = bayer4x4(int(gl_FragCoord.x), int(gl_FragCoord.y));
    if (a < threshold) discard;
    // depth-only
}

