#version 430 core

uniform vec3 lightColor;
uniform int timeValue;
uniform vec3 viewPos;
uniform sampler2DArray textureArray;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

vec3 computeSunPosition(float time) {
    float pi = 3.14159265359;
    float radius = 1500.0;
    float angle = (time / 86400.0) * (2.0 * pi);
    float tiltAngle = radians(30.0);

    float x = radius * sin(angle) * cos(tiltAngle);
    float y = radius * sin(angle);
    float z = radius * cos(angle);

    return vec3(x, y, z);
}

float calculateDiffuseLight(float time, vec3 lightDir) {
    vec3 norm = normalize(Normal);
    return max(dot(norm, lightDir), 0.0);
}

float calculateAmbientLight(float time) {
    float ambient = 0.2;

    if (time < 43200) {
        float angle = (time / 43200.0) * 3.14159265359;
        ambient += 0.15 * sin(angle);
    } else {
        time -= 43200;
        float angle = (time / 43200.0) * 3.14159265359;
        ambient += 0.5 * sin(angle);
    }
    return ambient;
}

float calculateSpecularLight(float time, vec3 lightDir) {
    vec3 norm = normalize(Normal);
    float specularStrength = (TextureID == 6) ? 0.1 : 0.2;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	float specular = specularStrength * spec;
	return specular;
}

void main() {
    vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
    vec3 lightDir = normalize(FragPos - computeSunPosition(timeValue));

    // Transparency for water
    if (TextureID == 6) {
        float minDistance = 20.0;
        float maxDistance = 250.0;
        float dist = distance(viewPos, FragPos);
        float transparency = clamp((dist - minDistance) / (maxDistance - minDistance), 0.6, 0.85);
        texColor.a *= transparency;
    }

    // Lighting strengths
    float ambient = calculateAmbientLight(timeValue);     // ~[0.2 - 0.7]
    float diffuse = calculateDiffuseLight(timeValue, lightDir); // ~[0.0 - 1.0]
    float specular = calculateSpecularLight(timeValue, lightDir); // ~[0.0 - 0.2]

    // Control how strong each contribution is
    float ambientWeight = 0.4;
    float diffuseWeight = 0.9;
    float specularWeight = 0.5;

    // Adjust diffuse factor by timeValue
    float diffuseFactor = 0.2;
    if (timeValue < 40000)
        diffuseFactor = 0.0;
    else if (timeValue <= 43200) {
        float time = timeValue - 40000;
        float angle = (time / 3200.0) * (3.14159265359 / 2.0);
        diffuseFactor = 0.2 * sin(angle);
    }

    // Combine lighting components
    float finalAmbient = ambient * ambientWeight;
    float finalDiffuse = diffuse * diffuseWeight * diffuseFactor;
    float finalSpecular = specular * specularWeight;

    float totalLight = clamp(finalAmbient + finalDiffuse + finalSpecular, 0.0, 1.0);
    vec3 result = totalLight * lightColor * texColor.rgb;

    FragColor = vec4(result, texColor.a);
}
