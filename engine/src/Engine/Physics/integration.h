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
		float const * _inv_masses,
		size_t const _count
	);

	void integrate_angular(
		float const _dt,
		glm::quat * _rotations,
		glm::vec3 * _angular_moments,
		glm::vec3 const * _torques,
		glm::mat3 const * _inv_inertial_tensors,
		size_t const _count
	);
}
}