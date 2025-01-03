/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:51:33 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/03 21:07:04 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "NoiseGenerator.hpp"

NoiseGenerator::NoiseGenerator(NoiseData noise_data) {
	data = noise_data;
	std::cout << "The seed for this world is: " << data.seed << std::endl;
}

NoiseGenerator::NoiseGenerator() {
	std::cout << "The seed for this world is: " << data.seed << std::endl;
}

NoiseGenerator::~NoiseGenerator() {

}

double NoiseGenerator::noise(double x, double y) {
	//Generate and calculate noise with given seed and offset
	(void)x;
	(void)y;
	return 0.0;
}
