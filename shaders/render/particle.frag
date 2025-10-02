#version 430 core

in vec4 vColor;
flat in int vKind;
out vec4 FragColor;

uniform int  timeValue;
uniform vec3 lightColor;

float calculateAmbientLight(float time) {
    const float pi = 3.14159265359;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
    float solar = sin(dayPhase * pi);
    return mix(0.10, 0.35, solar);
}

void main()
{
    if (vKind == 1) {
        // Water splashes: bias toward blue, avoid whitening
        vec3 water = vColor.rgb;
        float ambient = calculateAmbientLight(float(timeValue));
        // Light-blue highlight with small factor, then blue channel lift
        water = mix(water, vec3(0.50, 0.75, 1.00), 0.10);
        water *= vec3(0.95, 0.98, 1.08);
        // Mild ambient scaling only
        water *= mix(0.80, 1.08, ambient);
        FragColor = vec4(clamp(water, 0.0, 1.0), vColor.a);
    } else {
        float ambient = calculateAmbientLight(float(timeValue));
        float totalLight = clamp(ambient * 0.8 + 0.2, 0.0, 1.0);
        vec3 lit = vColor.rgb * lightColor * totalLight;
        // Subtle gamma tweak to reduce oversaturation
        lit = pow(max(lit, vec3(0.0)), vec3(0.95));
        FragColor = vec4(lit, vColor.a);
    }
}
