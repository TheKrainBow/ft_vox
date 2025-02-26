#version 430 core

uniform vec3 lightColor;
uniform int timeValue; // New time uniform
uniform vec3 viewPos;
uniform sampler2DArray textureArray;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos; // Store world position

out vec4 FragColor;

vec3 computeSunPosition(int time) {
    float radius = 100.0;
    float angle = radians(float(time)) / 5;
    return vec3(100.0, radius * sin(angle), radius * cos(angle));
}

void main() {
    vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));

    // Ambient Lighting
    float ambientStrength = 0.4;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse Lighting
    vec3 norm = normalize(Normal);
    vec3 sunPos = computeSunPosition(timeValue);
    vec3 lightDir = normalize(sunPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Combine lighting
    vec3 result = (ambient + diffuse) * texColor.rgb;

    FragColor = vec4(result, texColor.a);
}
