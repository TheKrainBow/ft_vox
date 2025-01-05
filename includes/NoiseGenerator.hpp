/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:49:04 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/05 14:50:42 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ft_vox.hpp"

class NoiseGenerator {
	public:
		NoiseGenerator(size_t seed);
		~NoiseGenerator();
		double noise(double x, double y) const;
		void setSeed(size_t seed);
	private:
		double singleNoise(double x, double y) const;
		double fade(double t) const;
		double lerp(double a, double b, double t) const;
		double grad(int hash, double x, double y) const;

		size_t _seed;
		size_t _nb_octaves = 4.0;
		double _lacunarity = 2.0;
		double _persistance = 0.5;
		std::vector<int> _permutation;
};
