/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Camera.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tmoragli <tmoragli@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/10/01 20:26:25 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/05 15:13:30 by tmoragli         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ft_vox.hpp"

class Camera {
	public:
		Camera() : position{0.0, -30.0, 0.0}, center{0.0f, 0.0f, 10.0f} {};
		~Camera() {};
		void move(float forward, float strafe, float up);
		void reset();
		void updateMousePos(int x, int y);

		vec3 position;
		vec3 center;
		float xangle = 0.0f;
		float yangle = 0.0f;
		float rotationspeed = 1.0f;
		float movementspeed = 0.5f;
		bool mouseRotation = false;
		vec2 mousePos;
};
