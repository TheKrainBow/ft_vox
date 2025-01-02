/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   NoiseGenerator.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/02 22:51:33 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/02 23:04:15 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "NoiseGenerator.hpp"

NoiseGenerator::NoiseGenerator(size_t seed) : _seed(seed) {
	std::cout << "The seed for this world is: " << _seed << std::endl;
}

NoiseGenerator::NoiseGenerator() : _seed(42) {
	std::cout << "The seed for this world is: " << _seed << std::endl;
}


NoiseGenerator::~NoiseGenerator() {

}

double NoiseGenerator::noise(double offset) {
	//Generate and calculate noise with given seed and offset
	(void)offset;
	return 0.0;
}
