/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:49:04 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/28 22:04:54 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ft_vox.hpp"
#include "define.hpp"
#include "SplineInterpolator.hpp"

struct NoiseData {
	double amplitude = 1.0;
	double frequency = 0.01;
	double persistance = 0.5;
	double lacunarity = 2.0;
	size_t nb_octaves = 4;
};

struct SplineData {
	SplineInterpolator continentalSpline;
	SplineInterpolator erosionSpline;
};

class NoiseGenerator {
	public:
		struct PerlinMap {
			double *map = nullptr;
			vec2	position;
			double	heighest = std::numeric_limits<double>::min();
			double	resolution = 1;
			double	size = CHUNK_SIZE;
			~PerlinMap() {if (map) delete [] map;};
		};
	public:
		NoiseGenerator(size_t seed);
		~NoiseGenerator();
		double noise(double x, double y) const;
		NoiseGenerator::PerlinMap *addPerlinMap(int x, int y, int size, int resolution);
		PerlinMap *getPerlinMap(int x, int y);
		void clearPerlinMaps(void);
		void setSeed(size_t seed);
		const size_t &getSeed() const;
		void setNoiseData(const NoiseData &data);
	private:
		double singleNoise(double x, double y) const;
		double fade(double t) const;
		double lerp(double a, double b, double t) const;
		double grad(int hash, double x, double y) const;
		double getContinentalNoise(vec2 pos);
		double getErosionNoise(vec2 pos);
		int getHeight(vec2 pos);
		vec2 getBorderWarping(double x, double z) const;

		size_t _seed;
		NoiseData _data;
		std::vector<int> _permutation;
		std::vector<PerlinMap *> _perlinMaps;
		SplineData spline;
};
