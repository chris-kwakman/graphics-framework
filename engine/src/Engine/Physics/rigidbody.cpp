#include "rigidbody.h"
#include <glm/gtx/matrix_cross_product.hpp>

namespace Engine {
namespace Physics {

	void integrate_linear(
		float const _dt, 
		glm::vec3* _positions, 
		glm::vec3* _velocities, 
		glm::vec3 const* _forces, 
		size_t const _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			_velocities[i] += _forces[i] * _dt;
			_positions[i] += _velocities[i] * _dt;
		}
	}

	void integrate_angular(
		float const _dt,
		glm::quat* _rotations, 
		glm::vec3 const* _angular_velocities, 
		size_t const _count
	)
	{
		for (size_t i = 0; i < _count; i++)
		{
			glm::quat const q_omega(0.0f,_angular_velocities[i].x, _angular_velocities[i].y, _angular_velocities[i].z);
			glm::quat const q_dot = 0.5f * q_omega * _rotations[i];
			_rotations[i] += q_dot * _dt;
		}
	}

	void integrate_angular(
		float const _dt,
		glm::mat3* _rotation_matrices, 
		glm::vec3 const * _angular_velocities, 
		size_t const _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			// Compute cross product using symmetric skew-matrix.
			_rotation_matrices[i] += (glm::matrixCross3(_angular_velocities[i]) * _rotation_matrices[i]) * _dt;
		}
	}

}
}