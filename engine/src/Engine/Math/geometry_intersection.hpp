#pragma once

#include "geometry.hpp"

namespace Engine {
namespace Math {

	char intersect_ray_sphere(ray _r, sphere _sphere, float & _t_min, float & _t_max);

	inline bool intersect_aabb_aabb(aabb const& _a1, aabb const& _a2)
	{
		glm::vec3 const a1_min = _a1.center - _a1.extent;
		glm::vec3 const a1_max = _a1.center + _a1.extent;
		glm::vec3 const a2_min = _a2.center - _a2.extent;
		glm::vec3 const a2_max = _a2.center + _a2.extent;
		return
			(a1_min.x <= a2_max.x && a1_max.x >= a2_min.x) &&
			(a1_min.y <= a2_max.y && a1_max.y >= a2_min.y) &&
			(a1_min.z <= a2_max.z && a1_max.z >= a2_min.z);
	}

}
}