#pragma once

#include <engine/Physics/convex_hull.h>
#include <Engine/Math/Transform3D.h>

namespace Engine {
	using namespace Math;

namespace Physics {

	enum EIntersectionType : char { eNoIntersection = 0, eEdgeIntersection = 1, eFaceIntersection = 2, eAnyIntersection = 3 };

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

	intersection_result intersect_ray_half_edge_data_structure(ray _ray, half_edge_data_structure const & _hds, transform3D const& _hull_transform);
	intersection_result intersect_ray_convex_polygon(ray const& _ray, glm::vec3 const* _vertices, size_t const _size);

	struct result_convex_hull_intersection
	{
		EIntersectionType intersection_type;
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
		half_edge_data_structure const& _hull1, transform3D _transform1, 
		half_edge_data_structure const& _hull2, transform3D _transform2
	);

	/*
	@brief	Internal method for intersecting two convex hulls
	@param		convex_hull		Convex hull 1
	@param		convex_hull		Convex hull 2
	@param		mat4			Matrix that transforms vertices of hull 2 to space of hull 1.
	@returns	result_convex_hull_intersection
	*/
	result_convex_hull_intersection intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, 
		half_edge_data_structure const& _hull2, 
		glm::mat4 const _mat_2_to_1
	);
}
}