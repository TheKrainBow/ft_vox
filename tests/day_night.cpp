#include "DayNightCycle.hpp"
#include <cassert>
#include <iostream>

static float brightness(const std::array<float,3>& color) {
	return color[0]*0.2126f+color[1]*0.7152f+color[2]*0.0722f;
}
int main() {
	auto noon = dayNightCycle(64200), midnight = dayNightCycle(21000);
	assert(noon.sunDirection[1]>0.999f && noon.moonVisibility==0);
	assert(midnight.moonDirection[1]>0.999f && midnight.sunVisibility==0);
	assert(brightness(midnight.skyColor)<brightness(noon.skyColor)*0.04f);
	assert(MOON_LIGHT_STRENGTH>0 && MOON_LIGHT_STRENGTH<0.1f);
	for (int t=-100; t<=86500; ++t) {
		auto cycle = dayNightCycle(t), next = dayNightCycle(t+1);
		float norm = 0;
		for (int i=0; i<3; ++i) {
			assert(cycle.sunDirection[i]==-cycle.moonDirection[i]);
			norm += cycle.sunDirection[i]*cycle.sunDirection[i];
			assert(cycle.skyColor[i]>=0 && cycle.skyColor[i]<=1);
			assert(std::abs(cycle.skyColor[i]-next.skyColor[i])<0.001f);
			assert(std::abs(cycle.skyTopColor[i]-next.skyTopColor[i])<0.001f);
			assert(std::abs(cycle.skyHorizonColor[i]-next.skyHorizonColor[i])<0.001f);
		}
		assert(std::abs(norm-1)<0.0001f);
	}
	assert(dayNightCycle(0).skyColor==dayNightCycle(86400).skyColor);
	assert(dayNightCycle(-1).skyColor==dayNightCycle(86399).skyColor);
	auto dawn = dayNightCycle(42000), dusk = dayNightCycle(0);
	assert(dawn.skyColor[0]>midnight.skyColor[0] && dusk.skyColor[0]>midnight.skyColor[0]);
	assert(dawn.skyColor!=dusk.skyColor);
	assert(dusk.skyHorizonColor[0]>dusk.skyHorizonColor[2]*2);
	assert(dusk.skyTopColor[2]>dusk.skyTopColor[0]);
	std::cout << "Sky cycle continuity, wrapping, twilight and opposite moon orbit passed\n";
}
