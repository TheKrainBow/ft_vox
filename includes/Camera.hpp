/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Camera.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: krain <krain@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/10/01 20:26:25 by tmoragli          #+#    #+#             */
/*   Updated: 2025/01/02 04:07:58 by krain            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ft_vox.hpp"

class Camera {
	public:
		Camera() : position{0.0, 0.0, 0.0}, center{0.0f, 0.0f, -10.0f} {};
		~Camera() {};
		void move(float forward, float strafe, float up);
		void reset();
		void updateMousePos(int x, int y);

		vec3 position;
		vec3 center;
		float xangle = 0.0;
		float yangle = 0.0;
		float rotationspeed = 1;
		float movementspeed = 0.15;
		bool mouseRotation = false;
		vec2 mousePos;
};