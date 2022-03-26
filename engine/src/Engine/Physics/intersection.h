#pragma once

#include <engine/Physics/convex_hull.h>
#include <Engine/Math/Transform3D.h>
#include <Engine/ECS/entity.h>

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

	struct contact
	{
		glm::vec3	point; //world-space point
		float		penetration;
	};

	struct contact_manifold
	{
		using half_edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::face_idx;
		using vertex_idx = half_edge_data_structure::vertex_idx;

		std::vector<contact> contact_points{};
		std::vector<glm::vec3> debug_draw_points{};
		uint16_t hull1_element_idx, hull2_element_idx;
		bool is_edge_edge;
	};

	/*
	@brief	Test for intersection between two convex hulls
	@param		convex_hull		Convex hull 1
	@param		transform3D		Transform of convex hull 1
	@param		convex_hull		Convex hull 2
	@param		transform3D		Transform of convex hull 2
	@returns	result_convex_hull_intersection
	*/
	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, transform3D const & _transform1, 
		half_edge_data_structure const& _hull2, transform3D const & _transform2,
		contact_manifold * _out_contact_manifold = nullptr
	);

	/*
	@brief	Internal method for intersecting two convex hulls
	@param		convex_hull		Convex hull 1
	@param		convex_hull		Convex hull 2
	@param		mat4			Matrix that transforms vertices of hull 2 to space of hull 1.
	@returns	result_convex_hull_intersection
	*/
	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1,
		half_edge_data_structure const& _hull2, 
		transform3D const & _transform_1,
		transform3D const& _transform_2_to_1, 
		contact_manifold* _out_contact_manifold
	);

	void points_inside_planes(
		glm::vec3 const* _plane_vertices,
		glm::vec3 const* _plane_normals,
		size_t const _plane_count,
		glm::vec3 const * _p,
		size_t const _point_count,
		bool * _out_results
	);
	float intersect_segment_planes(
		glm::vec3 const _p1, glm::vec3 const _p2,
		glm::vec3 const* _plane_vertices,
		glm::vec3 const* _plane_normals,
		size_t const _plane_count
	);
}
}