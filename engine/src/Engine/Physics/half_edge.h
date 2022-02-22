#pragma once


#include <stdint.h>
#include <glm/vec3.hpp>
#include <vector>
#include <limits>

namespace Engine {
namespace Physics {

	/*
	* Specifications:
	* - One edge may only be adjacent to two faces at most.
	* -
	*/
	struct half_edge_data_structure
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

		glm::vec3 compute_face_normal(face_idx _face) const;
		half_edge_idx get_previous_edge(half_edge_idx _edge_idx) const;
	};

	/*
	* @brief	For every edge in the convex hull, set its twin edge (if it exists)
	* @param	std::vector<half_edge_data_structure::half_edge> &		List of edges to perform operation on.
	* @param	size_t				Number of specified edge indices to process
	* @param	half_edge_idx[]		Array of edge indices to perform operation on.
	* @details	If array is nullptr, perform on all indices.
	*/
	void pair_hds_twin_edges(
		std::vector<half_edge_data_structure::half_edge>& _edges,
		size_t _process_edge_count = 0,
		half_edge_data_structure::half_edge_idx const* _process_edges = nullptr
	);

	void merge_hds_features(
		half_edge_data_structure & _hds,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces,
		std::vector<glm::vec3> const& _face_normals
	);

	void delete_unused_hds_features(
		half_edge_data_structure& _hds,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces
	);

	/*
	@brief	Create a convex hull from a vertex array and face index array input.
	@param		glm::vec3 *		Array of vertices.
	@param		size_t			Size of vertex array.
	@param		glm::uvec3 *	Array of face indices.
	@param		size_t			Size of face indices array.
	@returns	half_edge_data_structure
	*/
	half_edge_data_structure construct_half_edge_data_structure(
		glm::vec3 const* _vertices, size_t _vertex_count,
		glm::uvec3 const* _face_indices, size_t _face_count
	);

	/*
	* @brief	View convex hull data in imgui widget
	* @param	half_edge_data_structure *	Convex hull data
	*/
	void DebugDisplayHalfEdgeDataStructure(
		half_edge_data_structure const* _hull
	);
}
}