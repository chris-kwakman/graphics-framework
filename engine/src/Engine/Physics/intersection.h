#pragma once

#include <engine/Physics/convex_hull.h>
#include <Engine/Math/Transform3D.h>
#include <Engine/ECS/entity.h>
#include <Engine/Math/geometry.hpp>

namespace Engine {
	using namespace Math;

namespace Physics {

	enum EIntersectionType : char { eNoIntersection = 0, eEdgeIntersection = 1, eFaceIntersection = 2, eAnyIntersection = 3 };

	struct contact;

	struct intersection_result
	{
		float t;
		uint32_t face_index;
		glm::vec3 normal;
	};

	intersection_result intersect_ray_convex_hull(Math::ray _ray, half_edge_data_structure const & _hds, transform3D const& _hull_transform);


	/*
	@brief	Test for intersection between two convex hulls
	@param		convex_hull		Convex hull 1
	@param		transform3D		Transform of convex hull 1
	@param		convex_hull		Convex hull 2
	@param		transform3D		Transform of convex hull 2
	@returns	result_convex_hull_intersection
	*/
	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, transform3D const & _transform1, uint16_t _entity_id_1, 
		half_edge_data_structure const& _hull2, transform3D const & _transform2, uint16_t _entity_id_2,
		contact* _out_contacts = nullptr, size_t* _out_contact_count = nullptr, 
		bool* _reference_is_hull1 = nullptr
	);

	void points_behind_planes(
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