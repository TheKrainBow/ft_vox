#version 430 core

in vec2 UV;
out vec4 FragColor;

uniform vec3 sunColor;
uniform float intensity; // e.g. 2.0 for glow

void main()
{
    float dist = length(UV - 0.5); // center fade
    float alpha = smoothstep(0.5, 0.0, dist); // radial falloff
    vec3 color = sunColor * intensity * alpha;

    FragColor = vec4(color, alpha);
}
