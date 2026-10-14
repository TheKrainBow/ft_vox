// Render the real water and fog fragment shaders on a headless EGL context.
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

static void require(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

static GLuint compile(GLenum type, const std::string& source)
{
	GLuint shader = glCreateShader(type);
	const char* text = source.c_str();
	glShaderSource(shader, 1, &text, nullptr);
	glCompileShader(shader);
	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[4096];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		std::cerr << log << '\n';
	}
	require(ok, "Shader compilation failed");
	return shader;
}

int main()
{
	auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
		eglGetProcAddress("eglGetPlatformDisplayEXT"));
	require(getDisplay != nullptr, "Surfaceless EGL unavailable");
	EGLDisplay display = getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
	require(eglInitialize(display, nullptr, nullptr), "EGL initialization failed");
	require(eglBindAPI(EGL_OPENGL_API), "OpenGL EGL binding failed");
	const EGLint configAttrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
	EGLConfig config;
	EGLint count;
	require(eglChooseConfig(display, configAttrs, &config, 1, &count) && count > 0,
		"EGL configuration unavailable");
	const EGLint contextAttrs[] = {EGL_CONTEXT_MAJOR_VERSION, 4,
		EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
		EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
	EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
	require(context != EGL_NO_CONTEXT, "OpenGL 4.3 context unavailable");
	require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context),
		"EGL context activation failed");

	const char* vertex = R"GLSL(#version 430 core
out vec2 TexCoord;
out vec2 texCoords;
out vec2 UV;
uniform vec2 celestialUV;
uniform int testTextureID;
flat out int TextureID;
out vec3 Normal;
out vec3 FragPos;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    TexCoord = texCoords = vec2(0.5);
    UV = celestialUV;
    TextureID = testTextureID;
    Normal = vec3(0, 1, 0);
    FragPos = vec3(0);
})GLSL";
	GLuint vs = compile(GL_VERTEX_SHADER,vertex);
	auto programFor = [&](const char* path) {
		std::ifstream file(path);
		require(file.is_open(), "Run the test from the repository root");
		std::stringstream source;
		source << file.rdbuf();
		GLuint fs = compile(GL_FRAGMENT_SHADER,source.str());
		GLuint program = glCreateProgram();
		glAttachShader(program,vs);
		glAttachShader(program,fs);
		glLinkProgram(program);
		GLint ok;
		glGetProgramiv(program,GL_LINK_STATUS,&ok);
		require(ok,"Shader link failed");
		glDeleteShader(fs);
		return program;
	};
	GLuint water = programFor("shaders/render/water.frag");
	GLuint fog = programFor("shaders/postProcess/fog.frag");
	GLuint filter = programFor("shaders/postProcess/underwater.frag");
	GLuint celestial = programFor("shaders/render/sun.frag");
	GLuint skyGradient = programFor("shaders/postProcess/skyGradient.frag");
	GLuint textures[7];
	glGenTextures(7,textures);
	const float scene[] = {0.2f,0.2f,0.2f,1.0f};
	const float normal[] = {0.5f,1.0f,0.5f,1.0f};
	const float reflection[] = {1,1,1,1};
	for (int unit : {0,2,3,6}) {
		glActiveTexture(GL_TEXTURE0+unit);
		glBindTexture(GL_TEXTURE_2D,textures[unit]);
		const float* pixel = unit == 2 ? normal : unit == 3 ? reflection : scene;
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,pixel);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	}
	for (int unit : {1,4}) {
		glActiveTexture(GL_TEXTURE0+unit);
		glBindTexture(GL_TEXTURE_2D,textures[unit]);
		float depth = 1;
		glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		if (unit == 4) {
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
		}
	}
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY,textures[5]);
	glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,7,0,GL_RGBA,GL_FLOAT,nullptr);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	GLuint fbo,vao;
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[6],0);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Framebuffer incomplete");
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);
	glViewport(0,0,1,1);
	GLuint active = water;
	auto loc = [&](const char* name) { return glGetUniformLocation(active,name); };
	const float identity[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
	for (GLuint program : {water,fog,filter,celestial,skyGradient}) {
		active = program;
		glUseProgram(active);
		glUniform2f(loc("celestialUV"),0.5f,0.5f);
		glUniform1i(loc("testTextureID"),6);
		glUniform1i(loc("screenTexture"),0);
		glUniform1i(loc("depthTexture"),1);
		glUniform1i(loc("normalMap"),2);
		glUniform1i(loc("planarTexture"),3);
		glUniform1i(loc("shadowMap"),4);
		glUniform1i(loc("textureArray"),5);
		glUniformMatrix4fv(loc("lightSpaceMatrix"),1,GL_FALSE,identity);
		for (const char* name : {"viewOpaque","projection","planarView","planarProjection"})
			glUniformMatrix4fv(loc(name),1,GL_FALSE,identity);
		glUniform1f(loc("nearPlane"),0.1f);
		glUniform1f(loc("farPlane"),9600);
		glUniform1f(loc("shadowNear"),1);
		glUniform1f(loc("shadowFar"),500);
		glUniform1f(loc("time"),64200);
		glUniform1f(loc("playerLightRadius"),10);
		glUniform3f(loc("playerLightPos"),0,0,0);
		glUniform3f(loc("lightColor"),1,1,1);
		glUniform3f(loc("sunDir"),0,1,0);
		glUniform1f(loc("moonLightStrength"),0.06f);
		glUniform3f(loc("skyColor"),0.53f,0.81f,0.92f);
		glUniform3f(loc("viewPos"),0,0.1f,0.2f);
		glUniform1f(loc("waterHeight"),0.8f);
		glUniform1f(loc("renderDistance"),20);
	}
	float lastPixel[4]{};
	auto render = [&](GLuint program, bool shadowed, bool player, bool submerged) {
		active = program;
		glUseProgram(active);
		glUniform1i(loc("playerLightEnabled"),player);
		glUniform1i(loc("isUnderwater"),submerged);
		glActiveTexture(GL_TEXTURE4);
		float depth = shadowed ? 0 : 1;
		glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
		glDrawArrays(GL_TRIANGLES,0,3);
		glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,lastPixel);
		require(glGetError()==GL_NO_ERROR,"GPU render error");
		return lastPixel[1];
	};
	for (bool submerged : {false,true}) {
		float sun = render(water,false,false,submerged);
		float cave = render(water,true,false,submerged);
		float player = render(water,true,true,submerged);
		require(sun > cave*3,"Shadowed water must be substantially darker than exposed water");
		require(player > cave*2 && player < sun,"Player light must illuminate cave water without restoring full sunlight");
	}
	glUseProgram(water);
	glUniform3f(glGetUniformLocation(water,"sunDir"),0,-1,0);
	require(render(water,false,false,false)<0.05f,"Night water must lose sunlight illumination");
	const float moonlit = render(water,false,false,false);
	glUseProgram(water);
	glUniform1f(glGetUniformLocation(water,"moonLightStrength"),0);
	require(moonlit > render(water,false,false,false)+0.005f,
		"Moonlight must illuminate exposed water at night");
	glUseProgram(water);
	glUniform1f(glGetUniformLocation(water,"moonLightStrength"),0.06f);
	// A nearby cave wall: fog must work even with less than one block of immersion.
	glActiveTexture(GL_TEXTURE1);
	float distance = 4;
	float depth = (9600.0f - 0.1f*9600.0f/distance) / (9600.0f-0.1f);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
	float dry = render(fog,true,false,false);
	float caveFog = render(fog,true,false,true);
	float sunFog = render(fog,false,false,true);
	require(std::abs(dry-0.2f)<0.001f,"Nearby dry cave must remain unfogged");
	require(caveFog < dry-0.015f,"Shallow cave immersion must visibly apply underwater fog");
	require(sunFog > caveFog+0.025f,"Fog must not add bright sunlight tint inside a shadowed cave");
	// Same local immersion above sea level: fog must be independent of altitude.
	glUseProgram(fog);
	const float elevatedShadowTransform[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,-150,0,1};
	glUniformMatrix4fv(glGetUniformLocation(fog,"lightSpaceMatrix"),1,GL_FALSE,elevatedShadowTransform);
	glUniform3f(glGetUniformLocation(fog,"viewPos"),0,150.1f,0.2f);
	glUniform1f(glGetUniformLocation(fog,"waterHeight"),150.8f);
	require(std::abs(render(fog,true,false,true)-caveFog)<0.001f,"Local cave fog must work above sea level");
	// The final screen filter is independent of geometry depth and lighting.
	float dryFilter = render(filter,true,false,false);
	float wetFilter = render(filter,true,false,true);
	require(std::abs(dryFilter-0.2f)<0.001f,"Leaving water must remove the screen filter completely");
	require(wetFilter < dryFilter-0.04f,"Underwater screen filter must visibly tint even nearby surfaces");
	glActiveTexture(GL_TEXTURE0);
	const float black[] = {0,0,0,1};
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,black);
	require(render(filter,true,false,true)>0.005f,"An underwater cue must remain visible in a black cave");
	glUseProgram(water);
	glUniform1i(glGetUniformLocation(water,"fullBright"),true);
	require(render(water,true,false,false)>0.1f,"Full lighting must reveal water at night in a cave");
	glUseProgram(celestial);
	glUniform3f(glGetUniformLocation(celestial,"sunColor"),1,0.78f,0.25f);
	glUniform1f(glGetUniformLocation(celestial,"intensity"),1);
	const float sunDisc = render(celestial,false,false,false);
	glUseProgram(celestial);
	glUniform3f(glGetUniformLocation(celestial,"sunColor"),0.32f,0.52f,1.0f);
	glUniform1f(glGetUniformLocation(celestial,"intensity"),0.32f);
	const float moonDisc = render(celestial,false,false,false);
	require(moonDisc > 0.1f && moonDisc < sunDisc*0.5f,
		"The visible moon disc must be dimmer than the sun");
	// Both celestial bodies must have the same smooth halo falloff.
	glUseProgram(celestial);
	glUniform2f(glGetUniformLocation(celestial,"celestialUV"),0.8f,0.5f);
	const float moonHalo = render(celestial,false,false,false);
	glUseProgram(celestial);
	glUniform3f(glGetUniformLocation(celestial,"sunColor"),1,0.78f,0.25f);
	glUniform1f(glGetUniformLocation(celestial,"intensity"),1);
	const float sunHalo = render(celestial,false,false,false);
	require(sunHalo > 0 && sunHalo < sunDisc && moonHalo > 0 && moonHalo < moonDisc,
		"Both bodies must fade outward as soft halos");
	require(std::abs(sunHalo/sunDisc-moonHalo/moonDisc)<0.001f,
		"Sun and moon must share the same halo profile");
	// Real sky shader: a warm sunset horizon must grade into violet overhead.
	glUseProgram(skyGradient);
	glUniformMatrix4fv(glGetUniformLocation(skyGradient,"invProjection"),1,GL_FALSE,identity);
	const float identityRot[] = {1,0,0,0,1,0,0,0,1};
	glUniformMatrix3fv(glGetUniformLocation(skyGradient,"invViewRot"),1,GL_FALSE,identityRot);
	glUniform3f(glGetUniformLocation(skyGradient,"skyTopColor"),0.2f,0.1f,0.4f);
	glUniform3f(glGetUniformLocation(skyGradient,"skyHorizonColor"),1,0.4f,0.2f);
	const float horizon = render(skyGradient,false,false,false);
	const float upwardRot[] = {1,0,0,0,0,-1,0,1,0};
	glUseProgram(skyGradient);
	glUniformMatrix3fv(glGetUniformLocation(skyGradient,"invViewRot"),1,GL_FALSE,upwardRot);
	const float zenith = render(skyGradient,false,false,false);
	require(std::abs(horizon-0.4f)<0.001f && std::abs(zenith-0.1f)<0.001f,
		"Sky gradient must follow world elevation as the camera turns");
	glUseProgram(water);
	glUniform1i(glGetUniformLocation(water,"testTextureID"),15);
	glUniform1i(glGetUniformLocation(water,"fullBright"),false);
	glUniform3f(glGetUniformLocation(water,"viewPos"),0,0.1f,0.2f);
	const float lavaNear=render(water,true,false,false);
	require(lastPixel[0]>0.95f && lavaNear>0.25f && lastPixel[2]<0.05f && std::abs(lastPixel[3]-0.98f)<0.001f,
		"Near lava must be orange, self-lit and retain only ten percent of water transparency");
	glUseProgram(water);
	glUniform3f(glGetUniformLocation(water,"viewPos"),0,0,60);
	const float lavaFar=render(water,true,false,false);
	require(lavaFar<lavaNear-0.20f && lastPixel[0]>0.4f && lastPixel[0]<0.6f && std::abs(lastPixel[3]-0.99f)<0.001f,
		"Lava must fade toward red with distance while staying almost opaque");
	glUseProgram(fog);
	glUniform1i(glGetUniformLocation(fog,"isInLava"),true);
	render(fog,true,false,false);
	require(lastPixel[0]>0.55f && lastPixel[0]>lastPixel[1]*6 && lastPixel[2]<0.02f,
		"Eye immersion in lava must produce dense orange/red fog even without water immersion");
	glUseProgram(filter);
	glUniform1i(glGetUniformLocation(filter,"isInLava"),true);
	render(filter,true,false,false);
	require(lastPixel[0]>0.50f && lastPixel[1]<0.08f && lastPixel[2]<0.02f,
		"The final lava immersion filter must remain very opaque after sky/sun compositing");
	std::cout << "Lava appearance/opacity/fog, celestial halos, sky and water GPU regressions passed\n";
	glDeleteVertexArrays(1,&vao);
	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(7,textures);
	glDeleteProgram(water);
	glDeleteProgram(fog);
	glDeleteProgram(filter);
	glDeleteProgram(celestial);
	glDeleteProgram(skyGradient);
	glDeleteShader(vs);
	eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	eglDestroyContext(display,context);
	eglTerminate(display);
}
