#pragma once

#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat3x3.hpp>

namespace Engine {
namespace Physics {

	struct rigidbody_data
	{
		glm::quat	rotation;
		glm::vec3	position;
		float		inv_mass;
		glm::vec3	linear_momentum;
		glm::vec3	force_accumulator;
		glm::vec3	angular_momentum;
		glm::vec3	torque_accumulator;
		glm::mat3	inertial_tensor;
		glm::mat3	inv_inertial_tensor;
		float		restitution;

		float		get_mass() const;
		void		set_mass(float _mass);

		glm::vec3	get_linear_velocity() const;
		glm::vec3	get_angular_velocity() const;

		void		set_linear_velocity(glm::vec3 _velocity);
		void		set_angular_velocity(glm::vec3 _velocity);

		glm::mat3	get_world_tensor() const;
		glm::mat3	get_inv_world_tensor() const;
	};

}
}