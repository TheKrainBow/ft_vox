/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:49:04 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/03 21:06:55 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include "ft_vox.hpp"

struct NoiseData {
	size_t seed = 42;
	size_t nb_octaves = 4;
	double lacunarity = 2.0;
	double persistance = 0.5;
};

class NoiseGenerator
{
	public:
		NoiseGenerator();
		NoiseGenerator(NoiseData noise_data);
		~NoiseGenerator();
		double noise(double x, double y);

	private:
		NoiseData data;
		std::string _stream;
};
