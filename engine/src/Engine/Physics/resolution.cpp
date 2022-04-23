#include "resolution.hpp"
#include <glm/detail/qualifier.hpp>
#include "physics_manager.hpp"

namespace Engine {
namespace Physics {

	/*
	* @brief	Gauss-Seidel algorithm for contact and friction resolution between
	*			rigidbodies described by contact manifolds
	* @param	global_contact_data &		Contact data for all colliding rigidbodies
	* @param	unsigned int				Iterations for resolving contacts
	* @param	float						Delta time this frame
	* @param	float						Beta variable used for contact resolution
	* 
	* @details	8 resolution_iterations_penetration are used for resolving friction between rigidbodies.
	*/
	void compute_resolution_gauss_seidel(
		global_contact_data& _global_contact_data,
		unsigned int _contact_iterations, 
		unsigned int _friction_iterations, 
		float _dt, 
		float _beta
	)
	{
		constexpr size_t FRICTION_ITERATIONS = 16;

		auto const & physics_mgr = Singleton<ScenePhysicsManager>();

		struct precomputed_contact_data
		{
			glm::vec3 friction_u;
			float effective_mass_contact;
			glm::vec3 friction_v;
			float effective_mass_friction_u;
			float effective_mass_friction_v;
			float bias;
		};

		auto & all_contacts = _global_contact_data.all_contacts;
		auto const& all_contact_manifolds = _global_contact_data.all_contact_manifolds;
		std::vector<precomputed_contact_data> cached_contact_data(all_contacts.size());
		std::vector<contact_lambdas> cached_contact_lambdas(all_contacts.size());

		// Pre-compute effective masses for all contacts.
		{
			size_t manifold_end_contact_idx = 0;
			for (size_t manifold_idx = 0; manifold_idx < all_contact_manifolds.size(); manifold_idx++)
			{
				contact_manifold cm = all_contact_manifolds[manifold_idx];
				rigidbody_data rbA = cm.rigidbodies.first.GetRigidBodyData();
				rigidbody_data rbB = cm.rigidbodies.second.GetRigidBodyData();
				glm::vec3 vA = rbA.get_linear_velocity();
				glm::vec3 wA = rbA.get_angular_velocity();
				glm::vec3 vB = rbB.get_linear_velocity();
				glm::vec3 wB = rbB.get_angular_velocity();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();

				// Search for cached rigidbody interaction from previous frame.
				auto const& cache = _global_contact_data.cache;
				contact_manifold_data cached_manifold_data;
				bool cached_interaction_found = false;

				manifold_end_contact_idx += cm.data.contact_count;
				for (size_t contact_idx = cm.data.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
				{
					precomputed_contact_data pcd;
					contact & c = all_contacts[contact_idx];
					contact_lambdas& cfl = cached_contact_lambdas[contact_idx];

					// Precompute data required for contact resolution.

					glm::vec3 const rA = c.point - rbA.position;
					glm::vec3 const rB = c.point - rbB.position;
					glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
					glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);

					float const rel_contact_velocity = glm::dot(c.normal, -vA - glm::cross(wA, rA) + vB + glm::cross(wB, rB));
					float const restitution_bias = std::min(rbA.restitution, rbB.restitution) * rel_contact_velocity;
					pcd.bias = restitution_bias + (_beta - physics_mgr.slop) * (-c.penetration / _dt);

					pcd.effective_mass_contact =
						rbA.inv_mass + rbB.inv_mass +
						glm::dot(cross_rA_n, inv_world_tensor_A * cross_rA_n) +
						glm::dot(cross_rB_n, inv_world_tensor_B * cross_rB_n);

					// Precompute data required for friction resolution.
					constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);
					constexpr float epsilon = glm::epsilon<float>();
					bool const normal_up_parallel = glm::all(glm::epsilonEqual(c.normal, up, epsilon)) || glm::all(glm::epsilonEqual(c.normal, -up, epsilon));
					// Compute two vectors (u and v) that are tangent and bitangent to normal.
					pcd.friction_u = glm::normalize(normal_up_parallel
						? glm::cross(c.normal, glm::vec3(1.0f, 0.0f, 0.0f))
						: glm::cross(c.normal, glm::vec3(0.0f, 1.0f, 0.0f))
					);
					pcd.friction_v = glm::normalize(glm::cross(c.normal, pcd.friction_u));

					glm::vec3 const cross_rA_u = glm::cross(rA, pcd.friction_u);
					glm::vec3 const cross_rB_u = glm::cross(rB, pcd.friction_u);
					glm::vec3 const cross_rA_v = glm::cross(rA, pcd.friction_v);
					glm::vec3 const cross_rB_v = glm::cross(rB, pcd.friction_v);

					pcd.effective_mass_friction_u = rbA.inv_mass + rbB.inv_mass +
						glm::dot(cross_rA_u, inv_world_tensor_A * cross_rA_u) +
						glm::dot(cross_rB_u, inv_world_tensor_B * cross_rB_u);
					pcd.effective_mass_friction_v = rbA.inv_mass + rbB.inv_mass +
						glm::dot(cross_rA_v, inv_world_tensor_A * cross_rA_v) +
						glm::dot(cross_rB_v, inv_world_tensor_B * cross_rB_v);

					cfl.lambda_penetration = 0.0f;
					cfl.lambda_friction_u = 0.0f;
					cfl.lambda_friction_v = 0.0f;

					// Store precomputed contact data into array
					cached_contact_data[contact_idx] = pcd;
				}
			}
		}

		/*
		* Contact caching.
		*/
		if(physics_mgr.contact_caching)
		{
			auto const& cache = _global_contact_data.cache;
			size_t manifold_end_contact_idx = 0;
			for (size_t manifold_idx = 0; manifold_idx < all_contact_manifolds.size(); manifold_idx++)
			{
				contact_manifold cm = all_contact_manifolds[manifold_idx];
				rigidbody_data rbA = cm.rigidbodies.first.GetRigidBodyData();
				rigidbody_data rbB = cm.rigidbodies.second.GetRigidBodyData();
				glm::vec3 vA = rbA.get_linear_velocity();
				glm::vec3 wA = rbA.get_angular_velocity();
				glm::vec3 vB = rbB.get_linear_velocity();
				glm::vec3 wB = rbB.get_angular_velocity();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();

				// Search for cached rigidbody interaction from previous frame.
				contact_manifold_data cached_manifold_data;
				bool cached_interaction_found = cache.find_cached_pair_manifold_data(cm.rigidbodies, cached_manifold_data);

				// If this rigidbody pair has interacted before, perform O(n) search for contacts in
				// referenced cached contact sub-array and perform warm-start.
				if (cached_interaction_found)
				{
					manifold_end_contact_idx += cm.data.contact_count;
					for (size_t contact_idx = cm.data.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
					{
						contact_lambdas& cfl = cached_contact_lambdas[contact_idx];
						contact const& c = all_contacts[contact_idx];
						precomputed_contact_data const& pcd = cached_contact_data[contact_idx];

						glm::vec3 const rA = c.point - rbA.position;
						glm::vec3 const rB = c.point - rbB.position;
						glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
						glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);
						glm::vec3 const cross_rA_u = glm::cross(rA, pcd.friction_u);
						glm::vec3 const cross_rB_u = glm::cross(rB, pcd.friction_u);
						glm::vec3 const cross_rA_v = glm::cross(rA, pcd.friction_v);
						glm::vec3 const cross_rB_v = glm::cross(rB, pcd.friction_v);

						size_t const end_contact_idx = cached_manifold_data.first_contact_index + cached_manifold_data.contact_count;
						for (size_t cached_contact_idx = cached_manifold_data.first_contact_index; cached_contact_idx < end_contact_idx; cached_contact_idx++)
						{
							if (cache.identifiers[cached_contact_idx] == c.identifier)
							{
								// Copy cached lambda values into accumulator lambda values.
								// Treat new lambda value as delta lambda, and apply it directly.
								cfl = cache.lambdas[cached_contact_idx];
								cfl.lambda_friction_u = 0.0f;
								cfl.lambda_friction_v = 0.0f;

								float const delta_lambda_penetration = cfl.lambda_penetration;
								float const delta_lambda_friction_u = cfl.lambda_friction_u;
								float const delta_lambda_friction_v = cfl.lambda_friction_v;

								// Apply penetration and friction constraint force
								vA = vA + rbA.inv_mass * (-pcd.friction_u * delta_lambda_friction_u - pcd.friction_v * delta_lambda_friction_v - c.normal * delta_lambda_penetration);
								wA = wA + inv_world_tensor_A * (-cross_rA_u * delta_lambda_friction_u - cross_rA_v * delta_lambda_friction_v - cross_rA_n * delta_lambda_penetration);
								vB = vB + rbB.inv_mass * (pcd.friction_u * delta_lambda_friction_u + pcd.friction_v * delta_lambda_friction_v + c.normal * delta_lambda_penetration);
								wB = wB + inv_world_tensor_B * (cross_rB_u * delta_lambda_friction_u + cross_rB_v * delta_lambda_friction_v + cross_rB_n * delta_lambda_penetration);

								break;
							}
						}
					}
				}

				rbA.set_linear_velocity(vA);
				rbB.set_linear_velocity(vB);
				rbA.set_angular_velocity(wA);
				rbB.set_angular_velocity(wB);

				// Only set rigidbody data back to rigidbody component after we've gone over
				// all the contacts between this pair.
				cm.rigidbodies.first.SetRigidBodyData(rbA);
				cm.rigidbodies.second.SetRigidBodyData(rbB);
			}
		}

		// First pass for contact resolution
		for (size_t iteration = 0; iteration < _contact_iterations; iteration++)
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;

			for (size_t i = 0; i < all_contact_manifolds.size(); i++)
			{
				// Precompute common data.
				contact_manifold cm = all_contact_manifolds[i];
				rigidbody_data rbA = cm.rigidbodies.first.GetRigidBodyData();
				rigidbody_data rbB = cm.rigidbodies.second.GetRigidBodyData();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();
				glm::vec3 vA = rbA.get_linear_velocity();
				glm::vec3 wA = rbA.get_angular_velocity();
				glm::vec3 vB = rbB.get_linear_velocity();
				glm::vec3 wB = rbB.get_angular_velocity();

				manifold_end_contact_idx += cm.data.contact_count;
				for (size_t contact_idx = cm.data.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
				{
					precomputed_contact_data const pcd = cached_contact_data[contact_idx];
					contact_lambdas& cfl = cached_contact_lambdas[contact_idx];
					contact & c = all_contacts[contact_idx];
					glm::vec3 const rA = c.point - rbA.position;
					glm::vec3 const rB = c.point - rbB.position;
					glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
					glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);

					// Equal to relative velocity between points
					glm::vec3 relative_vel = -vA - glm::cross(wA, rA) + vB + glm::cross(wB, rB);

					//// Compute lambda for Baumgarte stabilization

					float const JV = glm::dot(c.normal, relative_vel);

					float delta_lambda_penetration = -((JV + pcd.bias) / pcd.effective_mass_contact);
					float const new_lambda_penetration = std::max(cfl.lambda_penetration + delta_lambda_penetration, 0.0f);
					delta_lambda_penetration = new_lambda_penetration - cfl.lambda_penetration;
					cfl.lambda_penetration = new_lambda_penetration;

					// Update lambdas in contact, and update rigidbody velocities.

					vA = vA + rbA.inv_mass * -c.normal * delta_lambda_penetration;
					wA = wA + inv_world_tensor_A * -cross_rA_n * delta_lambda_penetration;
					vB = vB + rbB.inv_mass * c.normal * delta_lambda_penetration;
					wB = wB + inv_world_tensor_B * cross_rB_n * delta_lambda_penetration;
				}

				rbA.set_linear_velocity(vA);
				rbB.set_linear_velocity(vB);
				rbA.set_angular_velocity(wA);
				rbB.set_angular_velocity(wB);

				// Only set rigidbody data back to rigidbody component after we've gone over
				// all the contacts between this pair.
				cm.rigidbodies.first.SetRigidBodyData(rbA);
				cm.rigidbodies.second.SetRigidBodyData(rbB);
			}
		}

		// Second pass for friction resolution
		for (size_t iteration = 0; iteration < _friction_iterations; iteration++)
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;

			for (size_t i = 0; i < all_contact_manifolds.size(); i++)
			{
				// Precompute common data.
				contact_manifold cm = all_contact_manifolds[i];
				rigidbody_data rbA = cm.rigidbodies.first.GetRigidBodyData();
				rigidbody_data rbB = cm.rigidbodies.second.GetRigidBodyData();

				glm::mat3 const inv_world_tensor_A = rbA.get_inv_world_tensor();
				glm::mat3 const inv_world_tensor_B = rbB.get_inv_world_tensor();
				glm::vec3 vA = rbA.get_linear_velocity();
				glm::vec3 wA = rbA.get_angular_velocity();
				glm::vec3 vB = rbB.get_linear_velocity();
				glm::vec3 wB = rbB.get_angular_velocity();

				float const friction_coefficient = std::min(rbA.friction_coefficient, rbB.friction_coefficient);

				manifold_end_contact_idx += cm.data.contact_count;
				for (size_t contact_idx = cm.data.first_contact_index; contact_idx < manifold_end_contact_idx; contact_idx++)
				{
					precomputed_contact_data const pcd = cached_contact_data[contact_idx];
					contact_lambdas& cfl = cached_contact_lambdas[contact_idx];
					contact& c = all_contacts[contact_idx];
					glm::vec3 const rA = c.point - rbA.position;
					glm::vec3 const rB = c.point - rbB.position;
					glm::vec3 const cross_rA_u = glm::cross(rA, pcd.friction_u);
					glm::vec3 const cross_rB_u = glm::cross(rB, pcd.friction_u);
					glm::vec3 const cross_rA_v = glm::cross(rA, pcd.friction_v);
					glm::vec3 const cross_rB_v = glm::cross(rB, pcd.friction_v);

					// Equal to relative velocity between points
					glm::vec3 relative_vel = -vA - glm::cross(wA, rA) + vB + glm::cross(wB, rB);

					float const constraint_fric_u = glm::dot(relative_vel, pcd.friction_u);
					float const constraint_fric_v = glm::dot(relative_vel, pcd.friction_v);

					float delta_lambda_friction_u = -(constraint_fric_u / pcd.effective_mass_friction_u);
					float delta_lambda_friction_v = -(constraint_fric_v / pcd.effective_mass_friction_v);

					float const max_friction_force = friction_coefficient * cfl.lambda_penetration;
					float const new_lambda_friction_u = std::clamp( cfl.lambda_friction_u + delta_lambda_friction_u, -max_friction_force, max_friction_force);
					float const new_lambda_friction_v = std::clamp( cfl.lambda_friction_v + delta_lambda_friction_v, -max_friction_force, max_friction_force);
					delta_lambda_friction_u = new_lambda_friction_u - cfl.lambda_friction_u;
					delta_lambda_friction_v = new_lambda_friction_v - cfl.lambda_friction_v;
					cfl.lambda_friction_u = new_lambda_friction_u;
					cfl.lambda_friction_v = new_lambda_friction_v;

					// Update lambdas in contact, and update rigidbody velocities.

					vA = vA + rbA.inv_mass * (-pcd.friction_u * delta_lambda_friction_u - pcd.friction_v * delta_lambda_friction_v);
					wA = wA + inv_world_tensor_A * (-cross_rA_u * delta_lambda_friction_u - cross_rA_v * delta_lambda_friction_v);
					vB = vB + rbB.inv_mass * (pcd.friction_u * delta_lambda_friction_u + pcd.friction_v * delta_lambda_friction_v);
					wB = wB + inv_world_tensor_B * (cross_rB_u * delta_lambda_friction_u + cross_rB_v * delta_lambda_friction_v);
				}

				rbA.set_linear_velocity(vA);
				rbB.set_linear_velocity(vB);
				rbA.set_angular_velocity(wA);
				rbB.set_angular_velocity(wB);

				// Only set rigidbody data back to rigidbody component after we've gone over
				// all the contacts between this pair.
				cm.rigidbodies.first.SetRigidBodyData(rbA);
				cm.rigidbodies.second.SetRigidBodyData(rbB);
			}
		}

		/*
		* Setup data for debug rendering
		*/

#ifdef DEBUG_RENDER_CONTACTS
		_global_contact_data.debug_resolved_contacts.resize(_global_contact_data.all_contacts.size());
		for (size_t i = 0; i < _global_contact_data.all_contacts.size(); i++)
		{
			resolved_contact new_resolved_contact;
			new_resolved_contact.contact = _global_contact_data.all_contacts[i];
			new_resolved_contact.contact_lambdas = cached_contact_lambdas[i];
			new_resolved_contact.vec_u = cached_contact_data[i].friction_u;
			new_resolved_contact.vec_v = cached_contact_data[i].friction_v;
			_global_contact_data.debug_resolved_contacts[i] = new_resolved_contact;
		}
#endif // DEBUG_RENDER_CONTACTS


		/*
		* Create cache
		*/

		if (physics_mgr.contact_caching)
		{
			contact_cache& cache = _global_contact_data.cache;
			cache = contact_cache{};
			for (auto& manifold : all_contact_manifolds)
				cache.manifolds.emplace(manifold.rigidbodies, manifold.data);
			cache.lambdas = std::move(cached_contact_lambdas);
			cache.identifiers.insert(cache.identifiers.begin(), all_contacts.begin(), all_contacts.end());
		}
		else
			_global_contact_data.cache = {};
	}

}
}