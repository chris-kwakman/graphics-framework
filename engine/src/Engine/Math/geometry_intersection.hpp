#pragma once

#include "geometry.hpp"

namespace Engine {
namespace Math {

	char intersect_ray_sphere(ray _r, sphere _sphere, float & _t_min, float & _t_max);

	inline bool intersect_aabb_aabb(aabb const& _a1, aabb const& _a2)
	{
		return glm::all(
			glm::greaterThanEqual(_a1.center + _a1.extent, _a2.center - _a2.extent) ||
			glm::lessThanEqual(_a1.center - _a1.extent, _a2.center + _a2.extent)
		);
	}

}
}