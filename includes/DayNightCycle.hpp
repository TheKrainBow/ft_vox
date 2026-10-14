#pragma once
#include <algorithm>
#include <array>
#include <cmath>

constexpr float MOON_LIGHT_STRENGTH = 0.06f;
struct DayNightState {
	std::array<float,3> sunDirection, moonDirection, skyColor, skyTopColor, skyHorizonColor;
	float sunVisibility, moonVisibility;
};
inline float cycleSmooth(float low, float high, float value) {
	float t = std::clamp((value-low)/(high-low),0.0f,1.0f);
	return t*t*(3.0f-2.0f*t);
}
inline DayNightState dayNightCycle(int time) {
	int t = (time%86400+86400)%86400;
	constexpr double pi = 3.14159265358979323846;
	double angle = t < 42000 ? pi + double(t)/42000*pi : double(t-42000)/44400*pi;
	float x = float(std::cos(angle)), y = float(std::sin(angle));
	DayNightState result{{x,y,0},{-x,-y,0},{},{},{},cycleSmooth(-0.02f,0.08f,y),cycleSmooth(-0.02f,0.08f,-y)};
	const std::array<float,3> nightTop{{0.006f,0.010f,0.025f}}, nightHorizon{{0.012f,0.020f,0.050f}};
	const std::array<float,3> dayTop{{0.22f,0.48f,0.80f}}, dayHorizon{{0.65f,0.84f,0.97f}};
	const std::array<float,3> violet{{0.23f,0.16f,0.40f}}, blueHourTop{{0.035f,0.040f,0.12f}};
	const std::array<float,3> dawn{{0.98f,0.56f,0.40f}}, sunset{{0.98f,0.43f,0.24f}}, blueHourHorizon{{0.26f,0.12f,0.30f}};
	float daylight = cycleSmooth(-0.24f,0.25f,y);
	float twilight = 1.0f-cycleSmooth(0.02f,0.25f,std::abs(y));
	float warmth = cycleSmooth(-0.22f,0.06f,y);
	for (int i=0; i<3; ++i) {
		float top = nightTop[i] + (dayTop[i]-nightTop[i])*daylight;
		float horizon = nightHorizon[i] + (dayHorizon[i]-nightHorizon[i])*daylight;
		float twilightTop = blueHourTop[i] + (violet[i]-blueHourTop[i])*warmth;
		float warmHorizon = x>0 ? dawn[i] : sunset[i];
		float twilightHorizon = blueHourHorizon[i] + (warmHorizon-blueHourHorizon[i])*warmth;
		result.skyTopColor[i] = top + (twilightTop-top)*twilight;
		result.skyHorizonColor[i] = horizon + (twilightHorizon-horizon)*twilight;
		result.skyColor[i] = result.skyTopColor[i]*0.55f + result.skyHorizonColor[i]*0.45f;
	}
	return result;
}
