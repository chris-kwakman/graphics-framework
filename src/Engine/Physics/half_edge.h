#pragma once

#include <stdint.h>
#include <glm/vec3.hpp>
#include <vector>
#include <memory>
#include <limits>

namespace Engine {
namespace Physics {

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
			vertex_idx		m_vertices[3];
			uint16_t		_meta;
		};

		std::vector<glm::vec3>	m_vertices;
		std::vector<half_edge>	m_edges;
		std::vector<face>		m_faces;
	};

	/*
	@brief	Create a convex hull from a vertex array and face index array input.
	@param		glm::vec3 *		Array of vertices.
	@param		size_t			Size of vertex array.
	@param		glm::uvec3 *	Array of face indices.
	@param		size_t			Size of face indices array.
	@returns	std::unique_ptr<convex_hull>
	*/
	std::unique_ptr<convex_hull> construct_convex_hull(
		glm::vec3* _vertices, size_t _vertex_count,
		glm::uvec3* _face_indices, size_t _face_count
	);

}
}