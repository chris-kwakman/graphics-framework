#include "resolution.hpp"
#include <glm/detail/qualifier.hpp>
#include "physics_manager.hpp"

namespace Engine {
namespace Physics {

	/*
	* @brief	Step in physics resolution to recover cached contact forces and re-apply them.
	* @param	global_contact_data			Contact data for all colliding rigidbodies.
	* @param	float						Delta time in simulation.
	* @param	physics_simulation_parameters
	* @param	contact_manifold[]			
	* @param	size_t						Size of contact manifold array.
	* @param	contact[]					
	* @param	precomputed_contact_data[]	1-1 mapping with contact array.
	* @param	size_t						Size of contact / precomputed_contact_data array.
	*/
	void precompute_contact_data_step(
		global_contact_data const& _global_contact_data,
		physics_simulation_parameters const & _parameters,
		contact_manifold const _contact_manifold_arr[],
		size_t const _contact_manifold_count,
		contact const _contact_arr[],
		precomputed_contact_data _out_precomputed_contact_data_arr[],
		size_t const _contact_count
	)
	{
		size_t manifold_end_contact_idx = 0;
		for (size_t manifold_idx = 0; manifold_idx < _contact_manifold_count; manifold_idx++)
		{
			contact_manifold cm = _contact_manifold_arr[manifold_idx];
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
				contact const & c = _contact_arr[contact_idx];

				// Precompute data required for contact resolution.

				glm::vec3 const rA = c.point - rbA.position;
				glm::vec3 const rB = c.point - rbB.position;
				glm::vec3 const cross_rA_n = glm::cross(rA, c.normal);
				glm::vec3 const cross_rB_n = glm::cross(rB, c.normal);

				float const rel_contact_velocity = glm::dot(c.normal, -vA - glm::cross(wA, rA) + vB + glm::cross(wB, rB));
				float const restitution_bias = std::min(rbA.restitution, rbB.restitution) * rel_contact_velocity;
				pcd.bias = restitution_bias + (_parameters.baumgarte - _parameters.slop) * (-c.penetration / _parameters.timestep);

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

				// Store precomputed contact data into array
				_out_precomputed_contact_data_arr[contact_idx] = pcd;
			}
		}
	}

	/*
	* @brief	Apply cached contact forces when available, and initialize contact lambdas
	*			to their appropriate value.
	* @param	global_contact_data				Contact data for all colliding rigidbodies.
	* @param	physics_simulation_parameters
	* @param	contact_manifold[]			
	* @param	size_t							Size of contact manifold array.
	* @param	contact[]					
	* @param	precomputed_contact_data[]		1-1 mapping with contact array.
	* @param	size_t							Size of contact array.
	*/
	void apply_cached_contact_forces_step(
		global_contact_data const &	_global_contact_data,
		physics_simulation_parameters const & _parameters,
		contact_manifold const _contact_manifold_arr[],
		size_t const _contact_manifold_count,
		contact const	_contact_arr[],
		precomputed_contact_data const _precomputed_contact_data_arr[],
		contact_lambdas _contact_lambdas_arr[],
		size_t const _contact_count
	)
	{
		auto const& cache = _global_contact_data.cache;
		size_t manifold_end_contact_idx = 0;
		for (size_t manifold_idx = 0; manifold_idx < _contact_manifold_count; manifold_idx++)
		{
			contact_manifold cm = _contact_manifold_arr[manifold_idx];
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
					contact_lambdas& cfl = _contact_lambdas_arr[contact_idx];
					contact const& c = _contact_arr[contact_idx];
					precomputed_contact_data const& pcd = _precomputed_contact_data_arr[contact_idx];

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

	/*
	* @brief	Iteratively resolve penetration constraints for contacts
	* @param	global_contact_data				Contact data for all colliding rigidbodies
	* @param	physics_simulation_parameters
	* @param	contact_manifold[]			
	* @param	size_t							Size of contact manifold array.
	* @param	contact[]					
	* @param	precomputed_contact_data[]		1-1 mapping with contact array.
	* @param	size_t							Size of contact array.
	*/
	void iterative_constraint_solver_penetration(
		global_contact_data const& _gobal_contact_data,
		physics_simulation_parameters const& _parameters,
		contact_manifold const _contact_manifold_arr[],
		size_t const _contact_manifold_count,
		contact const _contact_arr[],
		precomputed_contact_data const _precomputed_contact_data_arr[],
		contact_lambdas _contact_lambdas_arr[],
		size_t const _contact_count
	)
	{
		for (size_t iteration = 0; iteration < _parameters.resolution_iterations_penetration; iteration++)
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;

			for (size_t i = 0; i < _contact_manifold_count; i++)
			{
				// Precompute common data.
				contact_manifold cm = _contact_manifold_arr[i];
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
					precomputed_contact_data const pcd = _precomputed_contact_data_arr[contact_idx];
					contact_lambdas& cfl = _contact_lambdas_arr[contact_idx];
					contact const & c = _contact_arr[contact_idx];
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
	}

	/*
	* @brief	Iteratively resolve friction constraints for contacts
	* @param	global_contact_data				Contact data for all colliding rigidbodies
	* @param	physics_simulation_parameters
	* @param	contact_manifold[]
	* @param	size_t							Size of contact manifold array.
	* @param	contact[]
	* @param	precomputed_contact_data[]		1-1 mapping with contact array.
	* @param	size_t							Size of contact array.
	*/
	void iterative_constraint_solver_friction(
		global_contact_data const& _gobal_contact_data,
		physics_simulation_parameters const& _parameters,
		contact_manifold const _contact_manifold_arr[],
		size_t const _contact_manifold_count,
		contact const _contact_arr[],
		precomputed_contact_data const _precomputed_contact_data_arr[],
		contact_lambdas _contact_lambdas_arr[],
		size_t const _contact_count
	)
	{
		for (size_t iteration = 0; iteration < _parameters.resolution_iterations_friction; iteration++)
		{
			size_t contact_idx = 0;
			size_t manifold_end_contact_idx = 0;

			for (size_t i = 0; i < _contact_manifold_count; i++)
			{
				// Precompute common data.
				contact_manifold cm = _contact_manifold_arr[i];
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
					precomputed_contact_data const pcd = _precomputed_contact_data_arr[contact_idx];
					contact_lambdas& cfl = _contact_lambdas_arr[contact_idx];
					contact const & c = _contact_arr[contact_idx];
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
					float const new_lambda_friction_u = std::clamp(cfl.lambda_friction_u + delta_lambda_friction_u, -max_friction_force, max_friction_force);
					float const new_lambda_friction_v = std::clamp(cfl.lambda_friction_v + delta_lambda_friction_v, -max_friction_force, max_friction_force);
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
	}

	/*
	* @brief	Gauss-Seidel algorithm for contact and friction resolution between
	*			rigidbodies described by contact manifolds
	* @param	global_contact_data &			Contact data for all colliding rigidbodies
	* @param	float							Delta time this frame
	* @param	physics_simulation_parameters	Parameters of simulation
	* 
	* @details	8 resolution_iterations_penetration are used for resolving friction between rigidbodies.
	*/
	void compute_resolution_gauss_seidel(
		global_contact_data& _global_contact_data, 
		physics_simulation_parameters const & _parameters
	)
	{
		std::vector<precomputed_contact_data> vec_precomputed_contact_data(
			_global_contact_data.all_contacts.size()
		);
		std::vector<contact_lambdas> vec_contact_lambdas(
			_global_contact_data.all_contacts.size()
		);

		// Pre-compute repeatedly used data for all contacts.
		precompute_contact_data_step(
			_global_contact_data, _parameters,
			_global_contact_data.all_contact_manifolds.data(),
			_global_contact_data.all_contact_manifolds.size(),
			_global_contact_data.all_contacts.data(),
			vec_precomputed_contact_data.data(),
			_global_contact_data.all_contacts.size()
		);

		if(_parameters.contact_caching)
		{
			apply_cached_contact_forces_step(
				_global_contact_data,
				_parameters,
				_global_contact_data.all_contact_manifolds.data(),
				_global_contact_data.all_contact_manifolds.size(),
				_global_contact_data.all_contacts.data(),
				vec_precomputed_contact_data.data(),
				vec_contact_lambdas.data(),
				_global_contact_data.all_contacts.size()
			);
		}

		iterative_constraint_solver_penetration(
			_global_contact_data,
			_parameters,
			_global_contact_data.all_contact_manifolds.data(),
			_global_contact_data.all_contact_manifolds.size(),
			_global_contact_data.all_contacts.data(),
			vec_precomputed_contact_data.data(),
			vec_contact_lambdas.data(),
			_global_contact_data.all_contacts.size()
		);
		
		iterative_constraint_solver_friction(
			_global_contact_data,
			_parameters,
			_global_contact_data.all_contact_manifolds.data(),
			_global_contact_data.all_contact_manifolds.size(),
			_global_contact_data.all_contacts.data(),
			vec_precomputed_contact_data.data(),
			vec_contact_lambdas.data(),
			_global_contact_data.all_contacts.size()
		);

		/*
		* Setup data for debug rendering
		*/

#ifdef DEBUG_RENDER_CONTACTS
		_global_contact_data.debug_resolved_contacts.resize(_global_contact_data.all_contacts.size());
		for (size_t i = 0; i < _global_contact_data.all_contacts.size(); i++)
		{
			resolved_contact new_resolved_contact;
			new_resolved_contact.contact = _global_contact_data.all_contacts[i];
			new_resolved_contact.contact_lambdas = vec_contact_lambdas[i];
			new_resolved_contact.vec_u = vec_precomputed_contact_data[i].friction_u;
			new_resolved_contact.vec_v = vec_precomputed_contact_data[i].friction_v;
			_global_contact_data.debug_resolved_contacts[i] = new_resolved_contact;
		}
#endif // DEBUG_RENDER_CONTACTS


		/*
		* Create cache
		*/

		if (_parameters.contact_caching)
		{
			contact_cache& cache = _global_contact_data.cache;
			cache = contact_cache{};
			for (auto& manifold : _global_contact_data.all_contact_manifolds)
				cache.manifolds.emplace(manifold.rigidbodies, manifold.data);
			cache.lambdas = std::move(vec_contact_lambdas);
			cache.identifiers.insert(
				cache.identifiers.begin(), 
				_global_contact_data.all_contacts.begin(), 
				_global_contact_data.all_contacts.end()
			);
		}
		else
			_global_contact_data.cache = {};
	}

}
}