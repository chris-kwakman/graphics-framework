#include "geometry.hpp"
#include <glm/gtx/component_wise.hpp>

namespace Engine {
namespace Math {

	sphere create_sphere_encapsulating_aabb(aabb const _aabb)
	{
		sphere s;
		s.center = _aabb.center;
		s.radius = glm::compMax(glm::abs(_aabb.extent));
		return s;
	}

	aabb create_aabb_encapsulating_obb(obb const _obb)
	{
		glm::vec3 max_extent(-std::numeric_limits<float>::max());
		for (int i = -1; i <= 1; i += 2)
			for (int j = -1; j <= 1; j += 2)
				for (int k = -1; k <= 1; k += 2)
					max_extent = glm::max(max_extent, glm::abs(_obb.rotation * (_obb.aabb.extent * glm::vec3(i, j, k))));
		aabb b;
		b.center = _obb.aabb.center;
		b.extent = max_extent;
		return b;
	}

}
}