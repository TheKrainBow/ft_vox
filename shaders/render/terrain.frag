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

vec3 computeSunPosition(float time) {
	vec3 viewPos = cameraPos;
	const float pi = 3.14159265359;
	const float radius = 6000.0;
	const float dayStart = 42000.0;       // sunrise
	const float dayLen   = 86400.0 - dayStart; // 44400 (day duration)
	const float nightLen = dayStart;      // 42000 (night duration)

	float angle;
	if (time < dayStart) {
		// Night: traverse angle from pi..2pi across [0, dayStart)
		float phase = clamp(time / nightLen, 0.0, 1.0);
		angle = pi + phase * pi;
	} else {
		// Day: traverse angle from 0..pi across [dayStart, 86400]
		float phase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
		angle = phase * pi;
	}

	float x = radius * cos(angle);
	float y = radius * sin(angle); // y>0 during day, y<0 during night
	float z = 0.0f;

	return viewPos + vec3(x, y, z);
}

float calculateDiffuseLight(float time, vec3 lightDir) {
	vec3 norm = normalize(Normal);
	return max(dot(norm, lightDir), 0.0);
}

float calculateAmbientLight(float time) {
	// Day runs [42000, 86400], night runs [0, 42000]
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart; // 44400

	// Day phase: 0 at sunrise, 1 at sunset; clamped outside day
	float dayPhase = clamp((time - dayStart) / dayLen, 0.0, 1.0);
	// Smooth bell curve: 0 at sunrise/sunset, 1 at midday
	float solar = sin(dayPhase * pi);

	// Ambient ranges: darker nights, brighter days
	float nightAmbient = 0.10; // ambient at night
	float dayAmbient   = 0.35; // ambient at midday peak

	return mix(nightAmbient, dayAmbient, solar);
}

float calculateSpecularLight(float time, vec3 lightDir) {
	vec3 viewPos = cameraPos;
	vec3 norm = normalize(Normal);
	float specularStrength = (TextureID == 6) ? 0.1 : 0.2;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	float specular = specularStrength * spec;
	return specular;
}

void main() {
	vec3 viewPos = cameraPos;
	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	// Use direction from fragment to light (not the opposite)
	vec3 lightDir = normalize(computeSunPosition(timeValue) - FragPos);

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

	// Adjust diffuse factor by timeValue to match day window
	const float pi = 3.14159265359;
	const float dayStart = 42000.0;
	const float dayLen   = 86400.0 - dayStart; // 44400
	float dayPhase = clamp((timeValue - dayStart) / dayLen, 0.0, 1.0);
	float sun = sin(dayPhase * pi); // 0 at sunrise/sunset, 1 at midday
	// soften transitions with a small threshold to avoid popping at horizon
	sun = smoothstep(0.0, 0.15, sun);
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
