#include "rigidbody_data.hpp"

namespace Engine {
namespace Physics {


	float rigidbody_data::get_mass() const
	{
		return inv_mass <= 0.0f ? 0.0f : 1.0f / inv_mass;
	}

	void rigidbody_data::set_mass(float _mass)
	{
		if (_mass > 0.0f)
		{
			inv_mass = 1.0f / _mass;
			if (inv_mass <= 0.0f)
			{
				inertial_tensor = glm::mat3(1.0f);
				inv_inertial_tensor = glm::mat3(1.0f);
			}
		}
		else
		{
			inv_mass = 0.0f;
			inertial_tensor = glm::mat3(0.0f);
			inv_inertial_tensor = glm::mat3(0.0f);
		}
	}

	glm::vec3 Engine::Physics::rigidbody_data::get_linear_velocity() const
	{
		return linear_momentum * inv_mass;
	}

	glm::vec3 Engine::Physics::rigidbody_data::get_angular_velocity() const
	{
		return get_inv_world_tensor() * angular_momentum;
	}

	void Engine::Physics::rigidbody_data::set_linear_velocity(glm::vec3 _velocity)
	{
		linear_momentum = _velocity * get_mass();
	}

	void Engine::Physics::rigidbody_data::set_angular_velocity(glm::vec3 _velocity)
	{
		if (inv_mass > 0.0f)
			angular_momentum = get_world_tensor() * _velocity;
	}

	glm::mat3 rigidbody_data::get_world_tensor() const
	{
		glm::mat3 const rot_mat = glm::toMat3(rotation);
		glm::mat3 const inv_rot_mat = glm::transpose(rot_mat);
		return rot_mat * inertial_tensor * inv_rot_mat;
	}

	glm::mat3 rigidbody_data::get_inv_world_tensor() const
	{
		glm::mat3 const rot_mat = glm::toMat3(rotation);
		glm::mat3 const inv_rot_mat = glm::transpose(rot_mat);
		return rot_mat * inv_inertial_tensor * inv_rot_mat;
	}

}
}