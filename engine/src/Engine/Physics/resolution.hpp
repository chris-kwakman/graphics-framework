#pragma once

#include <Engine/Components/Rigidbody.h>
#include "rigidbody_data.hpp"
#include "contact.h"

namespace Engine {
namespace Physics {

	void compute_resolution_gauss_seidel(
		global_contact_data& _global_contact_data,
		unsigned int _iterations, 
		unsigned int _friction_iterations, 
		float _dt, 
		float _beta
	);

}
}