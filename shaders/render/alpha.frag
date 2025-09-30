#version 430 core

uniform vec3 lightColor;
uniform int timeValue;
uniform mat4 view;
uniform vec3 cameraPos; // explicit camera world position
uniform sampler2DArray textureArray;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

// Lighting helpers copied from terrain.frag
vec3 computeSunPosition(float time) {
    vec3 viewPos = cameraPos;
    const float pi = 3.14159265359;
    const float radius = 6000.0;
    const float dayStart = 42000.0;       // sunrise
    const float dayLen   = 86400.0 - dayStart; // 44400 (day duration)
    const float nightLen = dayStart;      // 42000 (night duration)

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

float calculateDiffuseLight(float time, vec3 lightDir) {
    vec3 norm = normalize(Normal);
    return max(dot(norm, lightDir), 0.0);
}

float calculateAmbientLight(float time) {
    const float pi = 3.14159265359;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart; // 44400
    float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
    float solar = sin(dayPhase * pi);
    float nightAmbient = 0.10;
    float dayAmbient   = 0.35;
    return mix(nightAmbient, dayAmbient, solar);
}

float calculateSpecularLight(float time, vec3 lightDir) {
    vec3 viewPos = cameraPos;
    vec3 norm = normalize(Normal);
    float specularStrength = 0.2;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
    float specular = specularStrength * spec;
    return specular;
}

void main() {
    // Only leaves reach this shader, but keep guard
    if (TextureID != 11) discard;

    vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
    // Masked alpha: discard mostly transparent texels so depth behaves like opaque
    if (texColor.a < 0.5) discard;

    vec3 viewPos = cameraPos;
    vec3 lightDir = normalize(computeSunPosition(timeValue) - FragPos);

    float ambient = calculateAmbientLight(timeValue);
    float diffuse = calculateDiffuseLight(timeValue, lightDir);
    float specular = calculateSpecularLight(timeValue, lightDir);

    float ambientWeight = 0.6;
    float diffuseWeight = 0.9;
    float specularWeight = 0.5;

    const float pi = 3.14159265359;
    const float dayStart = 42000.0;
    const float dayLen   = 86400.0 - dayStart;
    float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
    float sun = sin(dayPhase * pi);
    sun = smoothstep(0.0, 0.15, sun);
    float diffuseFactor = 0.2 * sun;

    float finalAmbient  = ambient * ambientWeight;
    float finalDiffuse  = diffuse * diffuseWeight * diffuseFactor;
    float finalSpecular = specular * specularWeight;
    float totalLight = clamp(finalAmbient + finalDiffuse + finalSpecular + 0.1, 0.0, 1.0);

    vec3 result = totalLight * lightColor * texColor.rgb;
    FragColor = vec4(result, 1.0);
}
