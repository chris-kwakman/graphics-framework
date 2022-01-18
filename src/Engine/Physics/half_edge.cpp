#include "half_edge.h"
#include <map>

namespace Engine {
namespace Physics {

	std::unique_ptr<convex_hull> construct_convex_hull(
		glm::vec3* _vertices,		size_t _vertex_count, 
		glm::uvec3* _face_indices,	size_t _face_count
	)
	{
		using half_edge = convex_hull::half_edge;
		using face = convex_hull::face;

		struct vertex_pair
		{
			uint16_t idx1, idx2;
			bool operator==(vertex_pair const& _p) const
			{
				return (idx1 == _p.idx1 && idx2 == _p.idx2) || (idx1 == _p.idx2 && idx2 == _p.idx1);
			}
		};

		convex_hull new_hull;
		new_hull.m_vertices.reserve(_vertex_count);
		memcpy(&new_hull.m_vertices.front(), _vertices, sizeof(glm::vec3)* _vertex_count);

		// Copy input faces into new hull
		new_hull.m_faces.resize(_face_count);
		for (uint16_t i = 0; i < _face_count; ++i)
		{
			face new_face;
			for (int j = 0; j < 3; ++j)
				new_face.m_vertices[j] = _face_indices[i][j];
			new_hull.m_faces.emplace_back(new_face);
		}

		/*
		Create half-edges for each face.
		*/
		decltype(new_hull.m_edges) naive_edges;
		naive_edges.reserve(_face_count * 3);
		// CCW half-edges
		for (uint16_t i = 0; i < _face_count; ++i)
		{
			glm::uvec3 const current_face_indices = _face_indices[i];
			uint16_t const current_edge_count = static_cast<uint16_t>(naive_edges.size());
			half_edge new_edges[3];
			for (uint16_t e = 0; e < 3; ++e)
			{
				new_edges[e].m_edge_face = i;
				new_edges[e].m_next_edge = current_edge_count + (e + 1)%3;
				new_edges[e].m_vertex = current_face_indices[e];
				//new_edges[e].m_twin_edge; - Will be set in a later pass.
			}
			naive_edges.emplace_back(new_edges[0]);
			naive_edges.emplace_back(new_edges[1]);
			naive_edges.emplace_back(new_edges[2]);
		}

		/*
		Find twin edges
		*/
		struct Vec3Less
		{
			bool operator()(glm::vec3 const & l, glm::vec3 const & r) const
			{
				return l.x < r.x || l.y < r.y || l.z < r.z;
			};
		};

		std::map<
			glm::vec3, 
			std::vector<convex_hull::half_edge_idx>, 
			Vec3Less
		> vertex_edge_map;

		// First pass to put edges belonging to same vertices in vectors belonging to those vertices.
		for (uint16_t e = 0; e < naive_edges.size(); ++e)
		{
			vertex_edge_map[new_hull.m_vertices[naive_edges[e].m_vertex]].emplace_back(e);
		}


		// For each vertex in the map, for each non-zero index half-edge, iterate over
		// its closed loop until you find a half-edge with a vertex that is the
		// end of the half-edge at index 0. Test if its' destination vertex is
		// the same vertex as that of the half-edge at index 0.

		// Skip half-edges that already have a twin-edge.
		for (auto vtx_edges_iter : vertex_edge_map)
		{
			// Assert that every vertex has at most one twin edge (since that is all we can support right now)
			assert(vtx_edges_iter.second.size() <= 2);

			std::vector<convex_hull::half_edge_idx> const& vtx_edge_indices = vtx_edges_iter.second;
			convex_hull::half_edge_idx main_edge_index = vtx_edge_indices[0];
			naive_edges[main_edge_index].m_twin_edge = convex_hull::half_edge::INVALID_EDGE;
			for (uint16_t e = 1; e < vtx_edge_indices.size(); ++e)
			{
				// ...
			}
		}


		return std::make_unique<convex_hull>(std::move(new_hull));
	}

}
}