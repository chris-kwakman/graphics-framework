#include "integration.h"
#include <glm/gtx/matrix_cross_product.hpp>

namespace Engine {
namespace Physics {

	void integrate_linear_euler(
		float const _dt,
		glm::vec3* _positions, 
		glm::vec3 * _linear_momentums,
		glm::vec3 const* _forces,
		float const* _inv_masses,
		size_t const _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			_linear_momentums[i] += _forces[i] * _dt;
			_positions[i] += _linear_momentums[i] * _inv_masses[i] * _dt;
		}
	}

	void integrate_angular_euler(
		float const _dt, 
		glm::quat* _rotations, // Assume normalized quaternions are passed.
		glm::vec3* _angular_moments, 
		glm::vec3 const* _torques, 
		glm::mat3 const* _inv_inertial_tensors,
		size_t const _count
	)
	{
		for (size_t i = 0; i < _count; i++)
		{
			// Cast quaternion to rotation matrices.
			glm::mat3 const rot_mat = glm::mat3_cast(_rotations[i]);
			// Use inverse = transpose property of orthonormal rotation matrices.
			glm::mat3 const inv_rot_mat = glm::transpose(rot_mat);

			glm::mat3 const world_inv_tensor = rot_mat * _inv_inertial_tensors[i] * inv_rot_mat;

			_angular_moments[i] += _torques[i] * _dt;

			glm::vec3 const omega = world_inv_tensor * _angular_moments[i];
			glm::quat const q_dot = 0.5f * glm::quat(0.0f, omega.x, omega.y, omega.z) * _rotations[i];
			_rotations[i] = glm::normalize(_rotations[i] + q_dot * _dt);
		}
	}

}
}