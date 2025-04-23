#version 430 core

uniform vec3 lightColor;
// New time uniform
uniform int timeValue;
uniform vec3 viewPos;
uniform sampler2DArray textureArray;

in vec2 TexCoord;
flat in int TextureID;
in vec3 Normal;
in vec3 FragPos; // Store world position

out vec4 FragColor;

vec3 computeSunPosition(float time) {
	float pi = 3.14159265359;
	float radius = 1500.0;
	// float angle = radians(float(time));
	float angle = (time / 86400) * (2 * pi);

	// Add a slight X-axis variation
	float tiltAngle = radians(30.0); // Adjust tilt for realism

	float x = radius * sin(angle) * cos(tiltAngle); // X rotation
	float y = radius * sin(angle); // Y rotation (up/down movement)
	float z = radius * cos(angle); // Z rotation (front/back movement)

	return vec3(x, y, z);
}

float calculateDifuseLight(float time, vec3 lightDir)
{
	vec3 norm = normalize(Normal);
	vec3 sunPos = computeSunPosition(time);
	float diff;
	diff = max(dot(norm, lightDir), 0.0);
	return (diff);
}

float calculateAmbientLight(float time)
{
	float ambient = 0.2;

	if (time < 43200) // Night time
	{
		float angle = (time / 43200) * 3.14159265359;
		float ambiantFactor = sin(angle);
		ambient += 0.15 * ambiantFactor;
	}
	else
	{
		time -= 43200;
		float angle = (time / 43200) * 3.14159265359;
		float ambiantFactor = sin(angle);
		ambient += 0.5 * ambiantFactor;
	}
	return (ambient);
}

float calculateSpecularLight(float time, vec3 lightDir)
{
	vec3 norm = normalize(Normal);
	float specularStrength;
	if (TextureID == 6 ) specularStrength = 0.1;
	else specularStrength = 0.2;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.1), 0.8);
	float specular = specularStrength * spec;
	return specular;
}

void main() {
	vec4 texColor = texture(textureArray, vec3(TexCoord, TextureID));
	vec3 lightDir = normalize(FragPos - computeSunPosition(timeValue));

	if (TextureID == 6)
	{
		// Transparency for water
		float minDistance = 20.0;
		float maxDistance = 250.0;
		float dist = distance(viewPos, FragPos);
		float transparency = clamp(((dist - minDistance) / (maxDistance - minDistance)), 0.6, 1.0);
		texColor.a = texColor.a * transparency;
	}

	// Ambient Lighting
	float ambientStrength = calculateAmbientLight(timeValue);

	// Specular Lighting
	float specularStrength = calculateSpecularLight(timeValue, lightDir);

	// Diffuse Lighting
	float diff = calculateDifuseLight(timeValue, lightDir);

	float diffuseFactor = 0.2;

	if (timeValue < 40000)
		diffuseFactor = 0;
	else if (timeValue >= 40000 && timeValue <= 43200)
	{
		float time = timeValue - 40000;
		float angle = (time / 3200) * (3.14159265359 / 2);
		diffuseFactor = sin(angle);
		diffuseFactor = 0.2 * diffuseFactor;
	}

	float totalLight = ambientStrength + specularStrength + (diff * diffuseFactor);
	if (totalLight > 1)
		totalLight = 1;
	// Combine lighting
	vec3 result = (totalLight * lightColor) * texColor.rgb;

	FragColor = vec4(result, texColor.a);
}
