#pragma once

#include <glm/vec3.hpp>
#include <engine/Components/Rigidbody.h>
#include <vector>

namespace Engine {
namespace Physics {

#define DEBUG_RENDER_CONTACTS

	struct contact
	{
		glm::vec3		point;			//world-space point. Originates from Object A.
		float			penetration;
		glm::vec3		normal;			// world-space normal. Originates from Object B.
	};

	struct contact_manifold
	{
		Component::RigidBody	rigidbody_A;			// Rigidbody creating point
		Component::RigidBody	rigidbody_B;			// Rigidbody creating normal (reference face object)
		uint32_t				first_contact_index;	// Index of first manifold contact in global contact array
		uint16_t				contact_count;			// Size of contact sequence in global contact array.
		bool					is_edge_edge;			// Whether manifold is edge-edge.
	};

	struct global_contact_data
	{
		std::vector<contact>			all_contacts;
		std::vector<contact_manifold>	all_contact_manifolds;

#ifdef DEBUG_RENDER_CONTACTS
		std::vector<glm::vec3>			debug_draw_points;
		std::vector<glm::vec3>			debug_draw_lines;
#endif // DEBUG_RENDER_CONTACTS
	};

}
}