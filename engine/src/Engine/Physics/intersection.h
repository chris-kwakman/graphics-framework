#pragma once

#include <engine/Physics/convex_hull.h>
#include <Engine/Math/Transform3D.h>

namespace Engine {
	using namespace Math;

namespace Physics {

	enum ECollideType : char { eNoCollision = 0, eEdgeCollision = 1, eFaceCollision = 2, eAnyCollision = 3 };

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

	struct result_convex_hull_intersection
	{
		ECollideType collision_type;
		union
		{
			struct edge_collision
			{
				convex_hull::half_edge_idx idx1, idx2;
			} edge_data;
			struct face_collision
			{
				convex_hull::face_idx idx1, idx2;
			} face_data;
		};
	};

	/*
	@brief	Test for intersection between two convex hulls
	@param		convex_hull		Convex hull 1
	@param		transform3D		Transform of convex hull 1
	@param		convex_hull		Convex hull 2
	@param		transform3D		Transform of convex hull 2
	@returns	result_convex_hull_intersection
	*/
	result_convex_hull_intersection intersect_convex_hulls_sat(
		convex_hull const& _hull1, transform3D _transform1, 
		convex_hull const& _hull2, transform3D _transform2
	);

	/*
	@brief	Internal method for intersecting two convex hulls
	@param		convex_hull		Convex hull 1
	@param		convex_hull		Convex hull 2
	@param		mat4			Matrix that transforms vertices of hull 2 to space of hull 1.
	@returns	result_convex_hull_intersection
	*/
	result_convex_hull_intersection intersect_convex_hulls_sat(
		convex_hull const& _hull1, convex_hull const& _hull2, glm::mat4 const _mat_2_to_1
	);
}
}