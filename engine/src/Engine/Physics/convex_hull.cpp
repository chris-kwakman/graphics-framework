#include "convex_hull.h"
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

#include <deque>

namespace Engine {
namespace Physics {

	// Helper function
	convex_hull::half_edge_idx get_previous_edge(convex_hull const& _hull, convex_hull::half_edge_idx _idx)
	{
		convex_hull::half_edge_idx edge_idx_iterator = _idx;
		convex_hull::half_edge iterator_edge = _hull.m_edges[edge_idx_iterator];
		do
		{
			edge_idx_iterator = iterator_edge.m_next_edge;
			iterator_edge = _hull.m_edges[edge_idx_iterator];
		} while (iterator_edge.m_next_edge != _idx);
		return edge_idx_iterator;
	}


	convex_hull construct_convex_hull(
		glm::vec3 const * _vertices,		size_t _vertex_count, 
		glm::uvec3 const * _face_indices,	size_t _face_count
	)
	{
		using half_edge = convex_hull::half_edge;
		using face = convex_hull::face;
		using half_edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::half_edge_idx;

		struct vertex_pair
		{
			vertex_pair(convex_hull::vertex_idx _idx1, convex_hull::vertex_idx _idx2) :
				idx1(std::min(_idx1, _idx2)), idx2(std::max(_idx1, _idx2))
			{}

			convex_hull::vertex_idx idx1, idx2;
			bool operator==(vertex_pair const& _p) const
			{
				return (idx1 == _p.idx1) && (idx2 == _p.idx2);
			}
			bool operator<(vertex_pair const& _p) const
			{
				return (idx1 == _p.idx1) ? idx2 > _p.idx2 : idx1 < _p.idx1;
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

		std::map<vertex_pair, std::vector<convex_hull::half_edge_idx>> vertex_edge_map;

		// First pass to put edges belonging to same vertices in vectors belonging to those vertices.
		for (uint16_t e = 0; e < naive_edges.size(); ++e)
		{
			vertex_pair edge_vertex_pair(
				naive_edges[e].m_vertex, 
				naive_edges[naive_edges[e].m_next_edge].m_vertex
			);
			vertex_edge_map[edge_vertex_pair].emplace_back(e);
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
				naive_edges[vertex_half_edges[0]].m_twin_edge = vertex_half_edges[1];
				naive_edges[vertex_half_edges[1]].m_twin_edge = vertex_half_edges[0];
			}
		}

		new_hull.m_edges = std::move(naive_edges);

		// Shorthand
		auto get_twin_index = [&](half_edge_idx _edge)->half_edge_idx& {return new_hull.m_edges[_edge].m_twin_edge; };
		auto get_next_index = [&](half_edge_idx _edge)->half_edge_idx& {return new_hull.m_edges[_edge].m_next_edge; };
		auto get_face_index = [&](half_edge_idx _edge)->face_idx& {return new_hull.m_edges[_edge].m_edge_face; };

		//// Face Merging
		
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

		for (half_edge_idx edge_index = 0; edge_index < existing_edges.size(); edge_index++)
		{
			half_edge const& current_edge = new_hull.m_edges[edge_index];
			face_idx const curr_edge_face_index = current_edge.m_edge_face;

			// Skip current iteration if edge has been deleted OR current edge has no twin edge.
			if (!existing_edges[edge_index] || current_edge.m_twin_edge == half_edge::INVALID_EDGE)
				continue;

			// Go over twin edge's closed loop.
			// Check if twin edge's face is coplanar.
			// If so, join face vertices and remove current edge and its twin.
			half_edge const& curr_twin_edge = new_hull.m_edges[current_edge.m_twin_edge];
			
			// If normalized normals do not point in same direction, they cannot be merged.
			if (!glm::all(glm::epsilonEqual(
				face_normals[curr_twin_edge.m_edge_face],
				face_normals[curr_edge_face_index], 
				0.00001f
			)))
				continue;

			// Collect sequence of edges including and after current edge whose twin faces are the same coplanar face.
			// However, we must also traverse the sequence BACKWARDS via the twin edges in the case that the
			// current edge is actually the middle of a sequence of edges.
			//	F0			F0			F0
			// NEXT <--- CURRENT <--- PREVIOUS
			//	F1			F1			F2
			std::deque<half_edge_idx> edges_sharing_face;
			{
				// Traverse forwards along side of current edge.
				half_edge_idx iterator_index = edge_index;
				half_edge iterator_edge = new_hull.m_edges[iterator_index];
				while (
					iterator_edge.m_twin_edge != half_edge::INVALID_EDGE &&
					get_face_index(iterator_edge.m_twin_edge) == curr_twin_edge.m_edge_face
				)
				{
					edges_sharing_face.emplace_back(iterator_index);
					iterator_index = iterator_edge.m_next_edge;
					iterator_edge = new_hull.m_edges[iterator_index];
				}
				// Traverse backwards along twin side of current edge.
				iterator_index = new_hull.m_edges[new_hull.m_edges[edge_index].m_twin_edge].m_next_edge;
				iterator_edge = new_hull.m_edges[iterator_index];
				while (
					iterator_edge.m_twin_edge != half_edge::INVALID_EDGE &&
					get_face_index(iterator_edge.m_twin_edge) == current_edge.m_edge_face
				)
				{
					edges_sharing_face.emplace_front(iterator_edge.m_twin_edge);
					iterator_index = iterator_edge.m_next_edge;
					iterator_edge = new_hull.m_edges[iterator_index];
				}
			}

			// Mark edges in sequence and their twins as deleted
			for (size_t i = 0; i < edges_sharing_face.size(); i++)
			{
				existing_edges[edges_sharing_face[i]] = false;
				existing_edges[get_twin_index(edges_sharing_face[i])] = false;
			}
			// Mark twin edge's face as deleted
			existing_faces[get_face_index(get_twin_index(edges_sharing_face.front()))] = false;

			// Get current edge's face, and get iterator to vertex index list
			convex_hull::face& current_face = new_hull.m_faces[current_edge.m_edge_face];
			auto face_vtx_iter = std::find(current_face.m_vertices.begin(), current_face.m_vertices.end(), current_edge.m_vertex);

			// Insert all vertices past current edge's origin vertex until we find edge whose twin is the back element in the edge sequence.
			convex_hull::half_edge_idx const twin_next_edge = new_hull.m_edges[current_edge.m_twin_edge].m_next_edge;
			convex_hull::half_edge_idx edge_iter = twin_next_edge;
			new_hull.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
			// Iterate over twin edge face until we find edge leading to current edge vertex
			while (new_hull.m_edges[edge_iter].m_next_edge != new_hull.m_edges[edges_sharing_face.back()].m_twin_edge)
			{
				edge_iter = new_hull.m_edges[edge_iter].m_next_edge;
				new_hull.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
				face_vtx_iter = current_face.m_vertices.insert(face_vtx_iter+1, new_hull.m_edges[edge_iter].m_vertex) + 1;
			}

			convex_hull::half_edge_idx previous_edge_idx = get_previous_edge(new_hull, edge_index);
			// Set each edge's previous edge's next edge to next edge of their corresponding twin edge.
			// (Very edgy)
			// AKA join loop formed by both face's edges, and remove edges between these two faces.
			new_hull.m_edges[previous_edge_idx].m_next_edge = twin_next_edge;
			new_hull.m_edges[edge_iter].m_next_edge = new_hull.m_edges[edges_sharing_face.back()].m_next_edge;
		}

		// Erase faces and edges that are marked for deletion.
		// This also involves updating all indices referring to them.

		// Generate an index to subtraction value map from indices that are going to be removed.
		std::vector<unsigned int> edge_index_substraction_map(existing_edges.size());
		edge_index_substraction_map[0] = (unsigned int)!existing_edges[0];
		for (size_t i = 1; i < edge_index_substraction_map.size(); i++)
			edge_index_substraction_map[i] = edge_index_substraction_map[i - 1] + (unsigned int)!existing_edges[i];

		std::vector<unsigned int> face_index_substraction_map(existing_faces.size());
		face_index_substraction_map[0] = (unsigned int)!existing_faces[0];
		for (size_t i = 1; i < face_index_substraction_map.size(); i++)
			face_index_substraction_map[i] = face_index_substraction_map[i - 1] + (unsigned int)!existing_faces[i];

		// New edges & faces array where indices marked for deletion are erased.
		decltype(new_hull.m_edges) new_edges;
		new_edges.reserve(existing_edges.size() - edge_index_substraction_map.back());
		decltype(new_hull.m_faces) new_faces;
		new_faces.reserve(existing_faces.size() - face_index_substraction_map.back());

		for (size_t i = 0; i < existing_edges.size(); i++)
		{
			if (existing_edges[i])
			{
				auto new_edge = new_hull.m_edges[i];
				new_edge.m_edge_face -= face_index_substraction_map[new_edge.m_edge_face];
				if(new_edge.m_twin_edge != convex_hull::half_edge::INVALID_EDGE)
					new_edge.m_twin_edge -= edge_index_substraction_map[new_edge.m_twin_edge];
				new_edge.m_next_edge -= edge_index_substraction_map[new_edge.m_next_edge];
				new_edges.emplace_back(new_edge);
			}
		}
		for (size_t i = 0; i < existing_faces.size(); i++)
		{
			if (existing_faces[i])
				new_faces.emplace_back(new_hull.m_faces[i]);
		}

		new_hull.m_edges = std::move(new_edges);
		new_hull.m_faces = std::move(new_faces);

		return new_hull;
	}

}
}