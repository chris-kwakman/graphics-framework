#pragma once

#include <glm/vec3.hpp>
#include <bitset>

namespace Engine {
namespace Math {

	struct simplex {
		uint32_t		vertex_indices;
		std::bitset<4>	active_vertices;
	};

}
}