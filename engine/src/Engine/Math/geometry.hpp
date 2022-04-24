#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Math {

	struct ray
	{
		glm::vec3 origin;
		glm::vec3 dir;
	};

	struct plane
	{
		glm::vec3	point;
		float		dot;
		glm::vec3	normal;
	};

	struct sphere
	{
		glm::vec3	center;
		float		radius;
	};

	struct aabb
	{
		glm::vec3	center;
		glm::vec3	extent;
	};

	struct obb
	{
		aabb		aabb;
		glm::quat	rotation;
	};


	sphere	create_sphere_encapsulating_aabb(aabb const _aabb);
	aabb	create_aabb_encapsulating_obb(obb const _obb);
}
}