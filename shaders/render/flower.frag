#version 460 core

in vec2 vUV;
flat in int vType;
in float vHeightMask;
in vec3 vWorldPos;

out vec4 FragColor;

uniform sampler2DArray flowerTex;
uniform int shortGrassLayer; // layer index for short_grass (greyscale), -1 if not present
uniform vec3  cameraPos;
uniform int   timeValue;
uniform vec3  lightColor;

vec3 computeSunPosition(float time) {
    vec3 viewPos = cameraPos;
    const float pi = 3.14159265359;
    const float radius = 6000.0;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    const float nightLen = dayStart;
    float angle;
    if (time < dayStart) {
        float phase = clamp(time / nightLen, 0.0, 1.0);
        angle = pi + phase * pi;
    } else {
        float phase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
        angle = phase * pi;
    }
    float x = radius * cos(angle);
    float y = radius * sin(angle);
    float z = 0.0f;
    return viewPos + vec3(x, y, z);
}

float calculateAmbientLight(float time) {
    const float pi = 3.14159265359;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
    float solar = sin(dayPhase * pi);
    float nightAmbient = 0.10;
    float dayAmbient   = 0.35;
    return mix(nightAmbient, dayAmbient, solar);
}

void main() {
    vec4 col = texture(flowerTex, vec3(vUV, float(vType)));
    if (col.a < 0.5) discard;

    // If this is the short grass layer, tint grayscale to green
    if (shortGrassLayer >= 0 && vType == shortGrassLayer) {
        // Use the red channel as intensity (greyscale source)
        float g = col.r;
        vec3 grass = vec3(0.38, 0.76, 0.36); // pleasant grass green
        col.rgb = grass * g;
    }

    // Upward facing diffuse to mimic ground-lit plants
    vec3 lightDir = normalize(computeSunPosition(float(timeValue)) - vWorldPos);
    float ambient = calculateAmbientLight(float(timeValue));
    float sun = max(lightDir.y, 0.0); // cosine vs up vector
    // Match terrain day scaling a bit
    const float pi = 3.14159265359;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    float dayPhase = clamp((float(timeValue) - dayStart) / dayLen, 0.0, 1.0);
    float sunPhase = smoothstep(0.0, 0.15, sin(dayPhase * pi));
    float diffuse = sun * 0.2 * sunPhase;
    float total = clamp(ambient * 0.6 + diffuse * 0.9 + 0.1, 0.0, 1.0);

    float ao = mix(0.9, 1.0, vHeightMask);
    vec3 rgb = col.rgb * ao * lightColor * total;
    FragColor = vec4(rgb, 1.0);
}
