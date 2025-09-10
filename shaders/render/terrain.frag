#version 430 core

uniform vec3 lightColor;
uniform int timeValue;
uniform mat4 view;
uniform sampler2DArray textureArray;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

vec3 computeSunPosition(float time) {
	vec3 viewPos = vec3(inverse(view)[3]);
	float pi = 3.14159265359;
	float radius = 6000.0;
	float angle = (time / 86400.0) * (2.0 * pi);
	float tiltAngle = radians(30.0);

	float x = radius * cos(angle);
	float y = -radius * sin(angle);
	float z = 0.0f;

	return viewPos + vec3(x, y, z);
}

float calculateDiffuseLight(float time, vec3 lightDir) {
	vec3 norm = normalize(Normal);
	return max(dot(norm, lightDir), 0.0);
}

float calculateAmbientLight(float time) {
    // Smooth, branchless day-night ambient using a cosine cycle
    // angle: 0 at midnight, pi at noon, 2pi next midnight
    const float pi = 3.14159265359;
    float angle = (time / 86400.0) * (2.0 * pi);

    // Map to [0,1]: 0 at midnight, 1 at noon
    float dayFactor = 0.5 - 0.5 * cos(angle);

    // Soften sunrise/sunset transitions
    dayFactor = smoothstep(0.0, 1.0, dayFactor);

    // Ambient ranges: darker nights, brighter days
    float nightAmbient = 0.10; // ambient at midnight
    float dayAmbient   = 0.35; // ambient at noon

    return mix(nightAmbient, dayAmbient, dayFactor);
}

float calculateSpecularLight(float time, vec3 lightDir) {
	vec3 viewPos = vec3(inverse(view)[3]);
	vec3 norm = normalize(Normal);
	float specularStrength = (TextureID == 6) ? 0.1 : 0.2;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	float specular = specularStrength * spec;
	return specular;
}

void main() {
	vec3 viewPos = vec3(inverse(view)[3]);
	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	vec3 lightDir = normalize(FragPos - computeSunPosition(timeValue));

	// Calculate the distance from the camera to the fragment
	float dist = distance(viewPos, FragPos);

	// Lighting strengths
	float ambient = calculateAmbientLight(timeValue);
	float diffuse = calculateDiffuseLight(timeValue, lightDir);
	float specular = calculateSpecularLight(timeValue, lightDir);

	// Control how strong each contribution is
	float ambientWeight = 0.6;
	float diffuseWeight = 0.9;
	float specularWeight = 0.5;

	// Adjust diffuse factor by timeValue
	const float pi = 3.14159265359;
	float angle = (timeValue / 86400.0) * (2.0 * pi);
	float sun = 0.5 - 0.5 * cos(angle); // 0 at midnight, 1 at noon
	// soften transitions; treat low sun as near-zero diffuse
	sun = smoothstep(0.05, 0.35, sun);
	float diffuseFactor = 0.2 * sun;

	// Combine lighting components
	float finalAmbient = ambient * ambientWeight;
	float finalDiffuse = diffuse * diffuseWeight * diffuseFactor;
	float finalSpecular = specular * specularWeight;

	float totalLight = clamp(finalAmbient + finalDiffuse + finalSpecular, 0.0, 1.0);
	vec3 result = totalLight * lightColor * texColor.rgb;
	
	// Face direction debug whitens the face depending on normal
	// if (Normal.x == -1)
	// 	result.rgb *= 10;
	FragColor = vec4(result, texColor.a);
}
