#pragma once

#include "half_edge.h"

#include <unordered_map>


namespace Engine {
namespace Physics {

	/*
	@brief	Create a convex hull from a vertex array using a 3D quickhull algorithm.
	@param		glm::vec3 *			Array of vertices
	@param		size_t				Size of vertex array
	@returns	half_edge_data_structure
	*/
	half_edge_data_structure construct_convex_hull(
		glm::vec3 const* _vertices, size_t const _vertex_count, size_t const _debug_iterations = std::numeric_limits<size_t>::max()
	);
	half_edge_data_structure construct_convex_hull(
		uint32_t _point_hull_handle, size_t _debug_iterations = std::numeric_limits<size_t>::max()
	);

	typedef uint16_t convex_hull_handle;

	class ConvexHullManager
	{
		struct convex_hull_info;

		convex_hull_handle m_handle_counter = 1;

	public:

		std::unordered_map<convex_hull_handle, convex_hull_info> m_map;

		struct convex_hull_info
		{
			half_edge_data_structure m_data;
			std::string m_name;
		};

		convex_hull_info const * GetConvexHullInfo(convex_hull_handle _handle) const;
		convex_hull_handle RegisterConvexHull(half_edge_data_structure&& _hull, std::string _name);
		void DeleteConvexHull(convex_hull_handle _handle);
	};
}
}