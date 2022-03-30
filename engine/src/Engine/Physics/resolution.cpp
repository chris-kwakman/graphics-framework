#include "resolution.hpp"
#include <glm/detail/qualifier.hpp>

namespace Engine {
namespace Physics {

	void compute_resolution_gauss_seidel(
		global_contact_data& _global_contact_data, 
		unsigned int _iterations,
		float _dt, float _beta
	)
	{
		struct precomputed_contact_data
		{
			float effective_mass;
			float bias;
		};

		auto & all_contacts = _global_contact_data.all_contacts;
		auto const& all_contact_manifolds = _global_contact_data.all_contact_manifolds;
		std::vector<precomputed_contact_data> cached_contact_data(all_contacts.size());

		// Pre-compute effective masses for all contacts.
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;
			for (size_t i = 0; i < all_contact_manifolds.size(); i++)
			{
				contact_manifold const cm = all_contact_manifolds[i];
				rigidbody_data const rbA = cm.rigidbody_A.GetRigidBodyData();
				rigidbody_data const rbB = cm.rigidbody_B.GetRigidBodyData();
				glm::vec3 const vA = rbA.get_linear_velocity();
				glm::vec3 const wA = rbA.get_angular_velocity();
				glm::vec3 const vB = rbB.get_linear_velocity();
				glm::vec3 const wB = rbB.get_angular_velocity();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();

				manifold_end_contact_idx += cm.contact_count;
				for (size_t contact_idx = cm.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
				{
					precomputed_contact_data pcd;
					contact & c = all_contacts[contact_idx];
					c.lambda = 0.0f;
					glm::vec3 const rA = c.point - rbA.position;
					glm::vec3 const rB = c.point - rbB.position;
					glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
					glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);
					float const rel_contact_velocity = glm::dot(c.normal, -vA - glm::cross(wA, rA) + vB + glm::cross(wB, rB));
					float const restitution_bias = std::min(rbA.restitution, rbB.restitution) * rel_contact_velocity;

					pcd.bias = restitution_bias + _beta * (c.penetration / _dt);
					pcd.effective_mass =
						rbA.inv_mass + rbB.inv_mass +
						glm::dot(cross_rA_n, inv_world_tensor_A * cross_rA_n) +
						glm::dot(cross_rB_n, inv_world_tensor_B * cross_rB_n);

					cached_contact_data[contact_idx] = pcd;
				}
			}
		}

		// Core Gauss-Seidel algorithm.
		for (size_t iteration = 0; iteration < _iterations; iteration++)
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;
			for (size_t i = 0; i < all_contact_manifolds.size(); i++)
			{
				// Precompute common data.
				contact_manifold cm = all_contact_manifolds[i];
				rigidbody_data rbA = cm.rigidbody_A.GetRigidBodyData();
				rigidbody_data rbB = cm.rigidbody_B.GetRigidBodyData();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();
				glm::vec3 vA = rbA.get_linear_velocity();
				glm::vec3 wA = rbA.get_angular_velocity();
				glm::vec3 vB = rbB.get_linear_velocity();
				glm::vec3 wB = rbB.get_angular_velocity();

				manifold_end_contact_idx += cm.contact_count;
				for (size_t contact_idx = cm.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
				{
					precomputed_contact_data const pcd = cached_contact_data[contact_idx];
					contact & c = all_contacts[contact_idx];
					glm::vec3 const rA = c.point - rbA.position;
					glm::vec3 const rB = c.point - rbB.position;
					glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
					glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);
					// Equal to relative contact velocity
					float const JV = glm::dot(c.normal, vA + glm::cross(wA, rA) - vB - glm::cross(wB, rB));

					float const lambda_c = -((JV + pcd.bias) / pcd.effective_mass);
					float const new_lambda = std::max(c.lambda + lambda_c, 0.0f);
					float const delta_lambda = c.lambda - new_lambda;

					vA = vA + rbA.inv_mass * -c.normal * delta_lambda;
					wA = wA + inv_world_tensor_A * -cross_rA_n * delta_lambda;
					vB = vB + rbB.inv_mass * c.normal * delta_lambda;
					wB = wB + inv_world_tensor_B * cross_rB_n * delta_lambda;

					c.lambda = new_lambda;
				}

				rbA.set_linear_velocity(vA);
				rbB.set_linear_velocity(vB);
				rbA.set_angular_velocity(wA);
				rbB.set_angular_velocity(wB);

				// Only set rigidbody data back to rigidbody component after we've gone over
				// all the contacts between this pair.
				cm.rigidbody_A.SetRigidBodyData(rbA);
				cm.rigidbody_B.SetRigidBodyData(rbB);
			}
		}
	}

}
}