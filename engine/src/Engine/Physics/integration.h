#pragma once

#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Engine {
namespace Physics {

	void integrate_linear(
		float const _dt, 
		glm::vec3 * _positions, 
		glm::vec3 * _velocities, 
		glm::vec3 const * _forces, 
		size_t const _count
	);
	
	// Need to consider inertial tensors and angular moments
	void integrate_angular(
		float const _dt,
		glm::mat3 * _rotation_matrices,
		glm::vec3 const * _angular_velocities,
		size_t const _count
	);

	void integrate_angular(
		float const _dt,
		glm::quat * _rotations,
		glm::vec3 const* _angular_velocities,
		size_t const _count
	);

	void apply_torque(
		glm::vec3* _angular_velocities,
		glm::vec3 const* _torques,
		size_t const _count
	);
}
}