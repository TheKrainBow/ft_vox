#include "SplineInterpolator.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>

SplineInterpolator::SplineInterpolator(const std::vector<Point>& pts) : points(pts) {
	setupData();
}

SplineInterpolator::SplineInterpolator() {}

void SplineInterpolator::setupData()
{
	if (points.size() < 2) 
	throw std::invalid_argument("At least two points are required for spline interpolation.");

	int n = points.size() - 1;
	h.resize(n);
	a.resize(n + 1);
	b.resize(n);
	c.resize(n + 1, 0.0);
	d.resize(n);

	// Step 1: Compute h and a
	for (int i = 0; i < n; ++i)
	{
		h[i] = points[i + 1].x - points[i].x;
		if (h[i] <= 0.0)
			throw std::invalid_argument("Points must have strictly increasing x values.");
		a[i] = points[i].y;
	}
	a[n] = points[n].y;

	// Step 2: Compute alpha
	std::vector<double> alpha(n, 0.0);
	for (int i = 1; i < n; ++i)
	{
		alpha[i] = (3.0 / h[i]) * (points[i + 1].y - points[i].y) -
					(3.0 / h[i - 1]) * (points[i].y - points[i - 1].y);
	}

	// Step 3: Setup tridiagonal matrix
	std::vector<double> l(n + 1, 0.0), mu(n, 0.0), z(n + 1, 0.0);
	l[0] = 1.0;
	l[n] = 1.0;
	z[0] = 0.0;
	z[n] = 0.0;

	for (int i = 1; i < n; ++i)
	{
		l[i] = 2.0 * (points[i + 1].x - points[i - 1].x) - h[i - 1] * mu[i - 1];
		if (l[i] == 0.0) throw std::runtime_error("Singular matrix detected.");
		mu[i] = h[i] / l[i];
		z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
	}

	// Step 4: Back substitution
	for (int j = n - 1; j >= 0; --j)
	{
		c[j] = z[j] - mu[j] * c[j + 1];
		b[j] = (points[j + 1].y - points[j].y) / h[j] - h[j] * (c[j + 1] + 2.0 * c[j]) / 3.0;
		d[j] = (c[j + 1] - c[j]) / (3.0 * h[j]);
	}
}



double SplineInterpolator::interpolate(double x) const {
	if (x < points.front().x || x > points.back().x)
		return 0.0;

	// Find the correct interval
	int n = points.size() - 1;
	int i = n - 1;
	for (int j = 0; j < n; ++j)
	{
		if (x <= points[j + 1].x)
		{
			i = j;
			break;
		}
	}

	// Compute spline value
	double dx = x - points[i].x;
	return a[i] + b[i] * dx + c[i] * dx * dx + d[i] * dx * dx * dx;
}

void SplineInterpolator::setPoints(const std::vector<Point> &pts)
{
	points = pts;
	setupData();
}
