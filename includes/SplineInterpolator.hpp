#pragma once
#include "ft_vox.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>

struct Point {
	double x, y;
};

class SplineInterpolator
{
	std::vector<Point> points;
	std::vector<double> a, b, c, d, h;
	public:
		SplineInterpolator(const std::vector<Point>& pts);
		SplineInterpolator();
		double interpolate(double x) const;
		void setPoints(const std::vector<Point>& pts);
	private:
		void setupData();
};
