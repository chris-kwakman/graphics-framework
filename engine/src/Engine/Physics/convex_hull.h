#pragma once

#include <stdint.h>
#include <glm/vec3.hpp>
#include <vector>
#include <limits>

#include <unordered_map>


namespace Engine {
namespace Physics {

	typedef uint16_t convex_hull_handle;

	/*
	* Specifications:
	* - One edge may only be adjacent to two faces at most.
	* - 
	*/
	struct convex_hull
	{
		typedef uint16_t half_edge_idx;
		typedef uint16_t vertex_idx;
		typedef uint16_t face_idx;

		struct half_edge
		{
			static half_edge_idx const INVALID_EDGE = std::numeric_limits<half_edge_idx>::max();

			half_edge_idx	m_next_edge;
			half_edge_idx	m_twin_edge; // It is assumed only one twin edge exists. Not true for more complex shapes.
			face_idx		m_edge_face;
			vertex_idx		m_vertex;
		};

		struct face
		{
			// TODO: Store all face vertex indices in one buffer, and
			// refer to it through an offset and a length (i.e. a bufferview)

			// Stored in counter-clockwise order.
			std::vector<half_edge_idx> m_edges;
			std::vector<vertex_idx>	m_vertices;
		};

		std::vector<glm::vec3>	m_vertices;
		std::vector<half_edge>	m_edges;
		std::vector<face>		m_faces;

		glm::vec3 compute_normal(face_idx _face) const;
	};

	/*
	@brief	Create a convex hull from a vertex array and face index array input.
	@param		glm::vec3 *		Array of vertices.
	@param		size_t			Size of vertex array.
	@param		glm::uvec3 *	Array of face indices.
	@param		size_t			Size of face indices array.
	@returns	convex_hull
	*/
	convex_hull construct_convex_hull(
		glm::vec3 const * _vertices,		size_t _vertex_count,
		glm::uvec3 const * _face_indices,	size_t _face_count
	);

	/*
	@brief	Create a convex hull from a vertex array using a 3D quickhull algorithm.
	@param		glm::vec3 *			Array of vertices
	@param		size_t				Size of vertex array
	@returns	convex_hull
	*/
	convex_hull construct_convex_hull(
		glm::vec3 const* _vertices, size_t const _vertex_count, size_t const _debug_iterations = std::numeric_limits<size_t>::max()
	);
	convex_hull construct_convex_hull(
		uint32_t _point_hull_handle, size_t _debug_iterations = std::numeric_limits<size_t>::max()
	);

	class ConvexHullManager
	{
		struct convex_hull_info;

		convex_hull_handle m_handle_counter = 1;

	public:

		std::unordered_map<convex_hull_handle, convex_hull_info> m_map;

		struct convex_hull_info
		{
			convex_hull m_data;
			std::string m_name;
		};

		convex_hull_info const * GetConvexHullInfo(convex_hull_handle _handle) const;
		convex_hull_handle RegisterConvexHull(convex_hull&& _hull, std::string _name);
		void DeleteConvexHull(convex_hull_handle _handle);
	};

	/*
	* @brief	View convex hull data in imgui widget
	* @param	convex_hull *	Convex hull data
	*/
	void DisplayConvexHullDataDebug(
		convex_hull const* _hull
	);
}
}