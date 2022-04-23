#pragma once

#include <Engine/Components/Rigidbody.h>
#include "rigidbody_data.hpp"
#include "contact.h"

namespace Engine {
namespace Physics {

	struct physics_simulation_parameters
	{
		float		 timestep = 1.0f / 60.0f;
		unsigned int subdivisions = 1;
		unsigned int resolution_iterations_penetration = 8;
		unsigned int resolution_iterations_friction = 8;
		float		 baumgarte = 0.2f;
		float		 slop = 0.02f;
		bool		 contact_caching = true;
	};

	struct precomputed_contact_data
	{
		glm::vec3 friction_u;
		float effective_mass_contact;
		glm::vec3 friction_v;
		float effective_mass_friction_u;
		float effective_mass_friction_v;
		float bias;
	};

	void compute_resolution_gauss_seidel(
		global_contact_data& _global_contact_data,
		physics_simulation_parameters const & _parameters
	);
}
}