#pragma once

#include <glm/vec3.hpp>
#include <engine/Components/Rigidbody.h>
#include <vector>

namespace Engine {
namespace Physics {

#define DEBUG_RENDER_CONTACTS

	struct contact_lambdas
	{
		float lambda_penetration = 0.0f;
		float lambda_friction_u = 0.0f;
		float lambda_friction_v = 0.0f;
	};

	/*
	* @brief
	* Data used for contact caching
	*/
	struct contact_identifier
	{
		uint16_t entity_id_1;
		uint16_t entity_id_2;
		uint16_t edge_index_1;
		uint16_t edge_index_2;

		inline bool operator==(contact_identifier const& _rhs) const = default;
	};

	struct contact
	{
		glm::vec3				point;			//world-space point. Originates from Object A.
		float					penetration;
		glm::vec3				normal;			// world-space normal. Originates from Object B.
		contact_identifier		identifier;		// Used for contact caching.

		operator contact_identifier() const { return identifier; }
	};

	using rigidbody_pair = std::pair<Component::RigidBody, Component::RigidBody>;

	struct rigidbody_pair_hash
	{
		std::size_t operator()(rigidbody_pair pair) const noexcept {
			using namespace Engine::ECS;
			std::hash<uint16_t> hasher;
			return hasher(pair.first.Owner().ID()) ^ hasher(pair.second.Owner().ID());
		}
	};

	struct contact_manifold_data
	{
		uint32_t				first_contact_index;	// Index of first manifold contact in global contact array
		uint16_t				contact_count;			// Size of contact sequence in global contact array.
		bool					is_edge_edge;			// Whether manifold is edge-edge.
	};

	struct contact_manifold
	{
		rigidbody_pair			rigidbodies;
		//Component::RigidBody	rigidbody_A;			// Rigidbody creating point
		//Component::RigidBody	rigidbody_B;			// Rigidbody creating normal (reference face object)
		contact_manifold_data	data;

		operator rigidbody_pair() const { return rigidbodies; }
		operator contact_manifold_data() const { return data; }
	};


#ifdef DEBUG_RENDER_CONTACTS
	struct resolved_contact
	{
		contact			contact;
		contact_lambdas contact_lambdas;
		glm::vec3		vec_u, vec_v;
	};
#endif // DEBUG_RENDER_CONTACTS


	struct contact_cache
	{
		std::unordered_map<
			rigidbody_pair, 
			contact_manifold_data,
			rigidbody_pair_hash
		>								manifolds;
		std::vector<contact_identifier>	identifiers;
		std::vector<contact_lambdas>	lambdas;

		bool find_cached_pair_manifold_data(rigidbody_pair _pair, contact_manifold_data & _out_data) const {
			auto iter = manifolds.find(_pair);
			bool result = (iter != manifolds.end());
			if (result) _out_data = iter->second;
			return result;
		}
	};

	struct global_contact_data
	{
		std::vector<contact>			all_contacts;
		std::vector<contact_manifold>	all_contact_manifolds;
		contact_cache					cache;

#ifdef DEBUG_RENDER_CONTACTS
		std::vector<glm::vec3>			debug_draw_points;
		std::vector<glm::vec3>			debug_draw_lines;
		std::vector<resolved_contact>	debug_resolved_contacts;
#endif // DEBUG_RENDER_CONTACTS
	};

}
}