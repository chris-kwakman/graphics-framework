#include "convex_hull.h"
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

namespace Engine {
namespace Physics {

	convex_hull construct_convex_hull(
		glm::vec3 const * _vertices,		size_t _vertex_count, 
		glm::uvec3 const * _face_indices,	size_t _face_count
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
		new_hull.m_vertices.resize(_vertex_count);
		memcpy(&new_hull.m_vertices.front(), _vertices, sizeof(glm::vec3)* _vertex_count);

		// Copy input faces into new hull
		new_hull.m_faces.reserve(_face_count);
		for (uint16_t face_index = 0; face_index < _face_count; ++face_index)
		{
			face new_face;
			new_face.m_vertices.resize(3);
			for (int vertex_index = 0; vertex_index < 3; ++vertex_index)
				new_face.m_vertices[vertex_index] = _face_indices[face_index][vertex_index];
			new_hull.m_faces.emplace_back(new_face);
		}

		/*
		Create half-edges for each face.
		*/
		decltype(new_hull.m_edges) naive_edges;
		naive_edges.reserve(_face_count * 3);
		// CCW half-edges
		for (uint16_t face_index = 0; face_index < _face_count; ++face_index)
		{
			glm::uvec3 const current_face_indices = _face_indices[face_index];
			uint16_t const current_edge_count = static_cast<uint16_t>(naive_edges.size());
			half_edge new_edges[3];
			for (uint16_t e = 0; e < 3; ++e)
			{
				new_edges[e].m_edge_face = face_index;
				new_edges[e].m_next_edge = current_edge_count + (e + 1)%3;
				new_edges[e].m_vertex = current_face_indices[e];
				new_edges[e].m_twin_edge = convex_hull::half_edge::INVALID_EDGE;
			}
			naive_edges.emplace_back(new_edges[0]);
			naive_edges.emplace_back(new_edges[1]);
			naive_edges.emplace_back(new_edges[2]);
		}

		/*
		Find twin edges
		*/

		std::map<convex_hull::vertex_idx, std::vector<convex_hull::half_edge_idx>> vertex_edge_map;

		// First pass to put edges belonging to same vertices in vectors belonging to those vertices.
		for (uint16_t e = 0; e < naive_edges.size(); ++e)
		{
			vertex_edge_map[naive_edges[e].m_vertex].emplace_back(e);
		}

		// For each vertex in the map, take the set of outgoing edges:
		// If there is one outgoing edge, mark that edge's twin as being non-existent.
		// If there are two outgoing edges, mark each edge's twin as being the other.
		// Cannot handle more than one twin edge right now.

		for (auto& pair : vertex_edge_map)
		{
			auto& [vertex_idx, vertex_half_edges] = pair;
			assert(vertex_half_edges.size() <= 2 && !vertex_half_edges.empty());

			// Set each half-edge as having the other half-edge as their twin (i.e. edge is adjacent to two faces)
			if (vertex_half_edges.size() == 2)
			{
				// Take second outgoing edge and find previous edge.
				// The previous edge is the first edge's twin edge, and vice versa.

				convex_hull::half_edge_idx edge_iterator = naive_edges[vertex_half_edges[1]].m_next_edge;
				while (vertex_half_edges[1] != naive_edges[edge_iterator].m_next_edge)
					edge_iterator = naive_edges[edge_iterator].m_next_edge;

				// Take second's edge previous edge, and mark its twin as the first edge. Vice versa.
				naive_edges[vertex_half_edges[0]].m_twin_edge = edge_iterator;
				naive_edges[edge_iterator].m_twin_edge = vertex_half_edges[0];
			}
		}

		new_hull.m_edges = std::move(naive_edges);

		
		// Second pass to merge coplanar faces.
		// The resulting face must be CONVEX.
		// Rather than checking for convexity, we assume
		// the input data will result in convex faces.

		// Indicates whether a face / an edge still exists.
		std::vector<bool> existing_faces(new_hull.m_faces.size(), true);
		std::vector<bool> existing_edges(new_hull.m_edges.size(), true);
		auto const& vertices = new_hull.m_vertices; // Define shorthand

		// Pre-calculate face normals to make my life easier.
		// Assume face vertices are defined in CCW order!
		std::vector<glm::vec3> face_normals(new_hull.m_faces.size());
		for (size_t i = 0; i < new_hull.m_faces.size(); ++i)
		{
			auto const& face_vtx_indices = new_hull.m_faces[i].m_vertices;
			face_normals[i] = glm::normalize(glm::cross(
				vertices[face_vtx_indices[1]] - vertices[face_vtx_indices[0]],
				vertices[face_vtx_indices[2]] - vertices[face_vtx_indices[1]]
			));
		}

		for (convex_hull::half_edge_idx edge_index = 0; edge_index < existing_edges.size(); edge_index++)
		{
			convex_hull::half_edge const& edge = new_hull.m_edges[edge_index];
			convex_hull::face_idx const edge_face_index = edge.m_edge_face;

			// Skip current iteration if edge has been deleted OR current edge has no twin edge.
			if (!existing_edges[edge_index] || edge.m_twin_edge == convex_hull::half_edge::INVALID_EDGE)
				continue;

			// Go over twin edge's closed loop.
			// Check if twin edge's face is coplanar.
			// If so, join face vertices and remove current edge and its twin.
			convex_hull::half_edge const& twin_edge = new_hull.m_edges[edge.m_twin_edge];
			
			// If normalized normals do not point in same direction, they cannot be merged.
			if (glm::all(glm::epsilonNotEqual(
				face_normals[twin_edge.m_edge_face],
				face_normals[edge_face_index], 
				0.00001f
			)))
				continue;

			// Mark edges as deleted
			existing_edges[edge_index] = false;
			existing_edges[edge.m_twin_edge] = false;

			// Mark twin edge's face as deleted
			existing_faces[twin_edge.m_edge_face] = false;

			// Update vertices belonging to current face.
			// ...

			convex_hull::vertex_idx const edge_destination_vertex = new_hull.m_edges[edge.m_vertex].m_vertex;
			convex_hull::half_edge_idx edge_iterator = edge.m_twin_edge;
			convex_hull::half_edge_idx previous_edge_index = convex_hull::half_edge::INVALID_EDGE;
			size_t edges_traversed = 0;
			do
			{
				previous_edge_index = edge_iterator;
				edge_iterator = new_hull.m_edges[edge_iterator].m_next_edge;
				edges_traversed += 1;
			} while (new_hull.m_edges[edge_iterator].m_vertex == edge_destination_vertex);
		}

		return new_hull;
	}

}
}