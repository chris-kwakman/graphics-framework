#pragma once

#include <engine/Physics/convex_hull.h>
#include <Engine/Math/Transform3D.h>

namespace Engine {
	using namespace Math;

namespace Physics {

	struct ray
	{
		glm::vec3 origin;
		glm::vec3 dir;
	};

	struct intersection_result
	{
		float t;
		uint32_t face_index;
		glm::vec3 normal;
	};

	intersection_result intersect_ray_convex_hull(ray _ray, convex_hull const& _hull, transform3D const& _hull_transform);
	intersection_result intersect_ray_convex_polygon(ray const& _ray, glm::vec3 const* _vertices, size_t const _size);

}
}