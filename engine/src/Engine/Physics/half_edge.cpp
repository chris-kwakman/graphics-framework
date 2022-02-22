#include "half_edge.h"

#include <map>
#include <algorithm>
#include <deque>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

namespace Engine {
namespace Physics {

	half_edge_data_structure::half_edge_idx half_edge_data_structure::get_previous_edge(half_edge_data_structure::half_edge_idx _idx) const
	{
		half_edge_data_structure::half_edge_idx edge_idx_iterator = _idx;
		half_edge_data_structure::half_edge iterator_edge = m_edges[edge_idx_iterator];
		do
		{
			edge_idx_iterator = iterator_edge.m_next_edge;
			iterator_edge = m_edges[edge_idx_iterator];
		} while (iterator_edge.m_next_edge != _idx);
		return edge_idx_iterator;
	}

	void pair_hds_twin_edges(
		std::vector<half_edge_data_structure::half_edge>& _ch_edges,
		size_t _process_edge_count,
		half_edge_data_structure::half_edge_idx const* _process_edges
	)
	{
		// Helper for creating vertex-pair map.
		struct vertex_pair
		{
			vertex_pair(half_edge_data_structure::vertex_idx _idx1, half_edge_data_structure::vertex_idx _idx2) :
				idx1(std::min(_idx1, _idx2)), idx2(std::max(_idx1, _idx2))
			{}

			half_edge_data_structure::vertex_idx idx1, idx2;
			bool operator==(vertex_pair const& _p) const
			{
				return (idx1 == _p.idx1) && (idx2 == _p.idx2);
			}
			bool operator<(vertex_pair const& _p) const
			{
				return (idx1 == _p.idx1) ? idx2 > _p.idx2 : idx1 < _p.idx1;
			}
		};

		/*
		Find twin edges
		*/

		std::map<vertex_pair, std::vector<half_edge_data_structure::half_edge_idx>> vertex_edge_map;

		// First pass to put edges belonging to same vertices in vectors belonging to those vertices.
		if (_process_edges)
		{
			for (size_t i = 0; i < _process_edge_count; i++)
			{
				half_edge_data_structure::half_edge_idx const e_idx = _process_edges[i];
				vertex_pair edge_vertex_pair(
					_ch_edges[e_idx].m_vertex,
					_ch_edges[_ch_edges[e_idx].m_next_edge].m_vertex
				);
				vertex_edge_map[edge_vertex_pair].emplace_back(e_idx);
			}
		}
		else
		{
			for (uint16_t e = 0; e < _ch_edges.size(); ++e)
			{
				vertex_pair edge_vertex_pair(
					_ch_edges[e].m_vertex,
					_ch_edges[_ch_edges[e].m_next_edge].m_vertex
				);
				vertex_edge_map[edge_vertex_pair].emplace_back(e);
			}
		}

		// For each vertex in the map, take the set of outgoing edges:
		// If there is one outgoing edge, mark that edge's twin as being non-existent.
		// If there are two outgoing edges, mark each edge's twin as being the other.

		for (auto& pair : vertex_edge_map)
		{
			auto& [vertex_idx, vertex_half_edges] = pair;
			assert(vertex_half_edges.size() <= 2 && !vertex_half_edges.empty());

			// Set each half-edge as having the other half-edge as their twin (i.e. edge is adjacent to two faces)
			if (vertex_half_edges.size() == 2)
			{
				_ch_edges[vertex_half_edges[0]].m_twin_edge = vertex_half_edges[1];
				_ch_edges[vertex_half_edges[1]].m_twin_edge = vertex_half_edges[0];
			}
		}
	}


	void merge_hds_features(
		half_edge_data_structure& _ch,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces,
		std::vector<glm::vec3> const& _face_normals
	)
	{
		using half_edge = half_edge_data_structure::half_edge;
		using face = half_edge_data_structure::face;
		using half_edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::half_edge_idx;

		auto get_twin_index = [&](half_edge_idx _edge)->half_edge_idx& {return _ch.m_edges[_edge].m_twin_edge; };
		auto get_next_index = [&](half_edge_idx _edge)->half_edge_idx& {return _ch.m_edges[_edge].m_next_edge; };
		auto get_face_index = [&](half_edge_idx _edge)->face_idx& {return _ch.m_edges[_edge].m_edge_face; };

		//// Face Merging

		// Second pass to merge coplanar faces.
		// The resulting face must be CONVEX.
		// Rather than checking for convexity, we assume
		// the input data will result in convex faces.

		for (half_edge_idx edge_index = 0; edge_index < _existing_edges.size(); edge_index++)
		{
			half_edge const& current_edge = _ch.m_edges[edge_index];
			face_idx const curr_edge_face_index = current_edge.m_edge_face;

			// Skip current iteration if edge has been deleted OR current edge has no twin edge.
			if (!_existing_edges[edge_index] || current_edge.m_twin_edge == half_edge::INVALID_EDGE)
				continue;

			// Go over twin edge's closed loop.
			// Check if twin edge's face is coplanar.
			// If so, join face vertices and remove current edge and its twin.
			half_edge const& curr_twin_edge = _ch.m_edges[current_edge.m_twin_edge];

			// If normalized normals do not point in same direction, they cannot be merged.
			if (!glm::all(glm::epsilonEqual(
				_face_normals[curr_twin_edge.m_edge_face],
				_face_normals[curr_edge_face_index],
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
				face_idx const current_edge_face = get_face_index(edge_index);
				face_idx const current_twin_edge_face = get_face_index(get_twin_index(edge_index));

				half_edge_idx forward_edge_iterator = edge_index;
				half_edge_idx backward_edge_iterator = get_next_index(get_twin_index(edge_index));

				// Traverse forwards along current edge's face.
				half_edge_idx iterator_twin = get_twin_index(forward_edge_iterator);
				while (
					iterator_twin != half_edge::INVALID_EDGE &&
					get_face_index(iterator_twin) == current_twin_edge_face
					)
				{
					edges_sharing_face.emplace_back(forward_edge_iterator);
					forward_edge_iterator = get_next_index(forward_edge_iterator);
					iterator_twin = get_twin_index(forward_edge_iterator);
				}
				// Traverse backwards along current twin edge's face.
				iterator_twin = get_twin_index(backward_edge_iterator);
				while (
					iterator_twin != half_edge::INVALID_EDGE &&
					get_face_index(iterator_twin) == current_edge_face
					) {
					edges_sharing_face.emplace_front(iterator_twin);
					backward_edge_iterator = get_next_index(backward_edge_iterator);
					iterator_twin = get_twin_index(backward_edge_iterator);
				}
			}

			// Mark edges in sequence and their twins as deleted
			for (size_t i = 0; i < edges_sharing_face.size(); i++)
			{
				_existing_edges[edges_sharing_face[i]] = false;
				_existing_edges[get_twin_index(edges_sharing_face[i])] = false;
			}
			// Mark twin edge's face as deleted
			_existing_faces[get_face_index(get_twin_index(edges_sharing_face.front()))] = false;

			// Insert all vertices past current edge's origin vertex until we find edge whose twin is the back element in the edge sequence.
			half_edge_data_structure::half_edge_idx const twin_next_edge = get_next_index(get_twin_index(edges_sharing_face.front()));
			half_edge_data_structure::half_edge_idx edge_iter = twin_next_edge;
			_ch.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
			// Iterate over twin edge face until we find edge leading to current edge vertex
			while (_ch.m_edges[edge_iter].m_next_edge != _ch.m_edges[edges_sharing_face.back()].m_twin_edge)
			{
				edge_iter = _ch.m_edges[edge_iter].m_next_edge;
				_ch.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
			}

			// Get edge previous to sequence of edges sharing face
			half_edge_data_structure::half_edge_idx previous_edge_idx = _ch.get_previous_edge(edges_sharing_face.front());
			// Set each edge's previous edge's next edge to next edge of their corresponding twin edge.
			// (Very edgy)
			// AKA join loop formed by both face's edges, and remove edges between these two faces.
			_ch.m_edges[previous_edge_idx].m_next_edge = get_next_index(get_twin_index(edges_sharing_face.front()));
			_ch.m_edges[edge_iter].m_next_edge = _ch.m_edges[edges_sharing_face.back()].m_next_edge;
		}

		//// Co-linear edge merging
		if (true)
		{
			// Pre-compute edge directions
			std::vector<glm::vec3> edge_normalized_dirs(_ch.m_edges.size());
			for (half_edge_idx edge_idx = 0; edge_idx < _ch.m_edges.size(); edge_idx++)
			{
				if (!_existing_edges[edge_idx])
					continue;

				edge_normalized_dirs[edge_idx] = glm::normalize(
					_ch.m_vertices[_ch.m_edges[get_next_index(edge_idx)].m_vertex] - _ch.m_vertices[_ch.m_edges[edge_idx].m_vertex]
				);
			}

			// For each edge, check if the current and next edge each have the same face on their sides.
			// Do the same for their twin edges.
			for (half_edge_idx edge_idx = 0; edge_idx < _ch.m_edges.size(); edge_idx++)
			{
				if (!_existing_edges[edge_idx])
					continue;

				half_edge_idx const next_idx = get_next_index(edge_idx);

				bool edges_colinear = glm::all(glm::epsilonEqual(edge_normalized_dirs[edge_idx], edge_normalized_dirs[next_idx], 0.001f));

				half_edge_idx const twin_idx = get_twin_index(edge_idx);
				half_edge_idx const next_twin_idx = get_twin_index(next_idx);
				face_idx const twin_face = (twin_idx == half_edge::INVALID_EDGE) ? half_edge::INVALID_EDGE : get_face_index(twin_idx);
				face_idx const next_twin_face = (next_twin_idx == half_edge::INVALID_EDGE) ? half_edge::INVALID_EDGE : get_face_index(next_twin_idx);

				bool edges_share_face = get_face_index(edge_idx) == get_face_index(next_idx);
				bool twin_edges_share_face = (twin_face == next_twin_face);

				if (edges_colinear && edges_share_face && twin_edges_share_face)
				{
					_existing_edges[next_idx] = false;
					_ch.m_edges[edge_idx].m_next_edge = get_next_index(next_idx);
				}
			}
		}
	}


	void delete_unused_hds_features(
		half_edge_data_structure& _ch,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces
	)
	{
		using half_edge = half_edge_data_structure::half_edge;
		using face = half_edge_data_structure::face;
		using half_edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::half_edge_idx;

		auto get_twin_index = [&](half_edge_idx _edge)->half_edge_idx& {return _ch.m_edges[_edge].m_twin_edge; };
		auto get_next_index = [&](half_edge_idx _edge)->half_edge_idx& {return _ch.m_edges[_edge].m_next_edge; };
		auto get_face_index = [&](half_edge_idx _edge)->face_idx& {return _ch.m_edges[_edge].m_edge_face; };

		// Re-generate face data from existing edge data.
		_ch.m_faces.clear();

		// Mark all vertices found in existing edges as existing.
		for (half_edge_idx edge_idx = 0; edge_idx < _ch.m_edges.size(); edge_idx++)
		{
			if (!_existing_edges[edge_idx])
				continue;

			_existing_vertices[_ch.m_edges[edge_idx].m_vertex] = true;
		}


		// Erase faces and edges that are marked for deletion.
		// This also involves updating all indices referring to them.

		// Generate an index to subtraction value map from indices that are going to be removed.
		std::vector<unsigned int> edge_index_substraction_map(_existing_edges.size());
		edge_index_substraction_map[0] = (unsigned int)!_existing_edges[0];
		for (size_t i = 1; i < edge_index_substraction_map.size(); i++)
			edge_index_substraction_map[i] = edge_index_substraction_map[i - 1] + (unsigned int)!_existing_edges[i];

		std::vector<unsigned int> face_index_substraction_map(_existing_faces.size());
		face_index_substraction_map[0] = (unsigned int)!_existing_faces[0];
		for (size_t i = 1; i < face_index_substraction_map.size(); i++)
			face_index_substraction_map[i] = face_index_substraction_map[i - 1] + (unsigned int)!_existing_faces[i];

		std::vector<unsigned int> vertex_index_substraction_map(_existing_vertices.size());
		vertex_index_substraction_map[0] = (unsigned int)!_existing_vertices[0];
		for (size_t i = 1; i < vertex_index_substraction_map.size(); i++)
			vertex_index_substraction_map[i] = vertex_index_substraction_map[i - 1] + (unsigned int)!_existing_vertices[i];

		// New edges & faces array where indices marked for deletion are erased.
		decltype(_ch.m_edges) new_edges;
		new_edges.reserve(_existing_edges.size() - edge_index_substraction_map.back());
		decltype(_ch.m_vertices) new_vertices;
		new_vertices.reserve(_existing_vertices.size() - vertex_index_substraction_map.back());
		_ch.m_faces.resize(_existing_faces.size() - face_index_substraction_map.back());

		for (size_t i = 0; i < _existing_edges.size(); i++)
		{
			if (_existing_edges[i])
			{
				auto new_edge = _ch.m_edges[i];
				new_edge.m_edge_face -= face_index_substraction_map[new_edge.m_edge_face];
				new_edge.m_vertex -= vertex_index_substraction_map[new_edge.m_vertex];
				if (new_edge.m_twin_edge != half_edge_data_structure::half_edge::INVALID_EDGE)
					new_edge.m_twin_edge -= edge_index_substraction_map[new_edge.m_twin_edge];
				new_edge.m_next_edge -= edge_index_substraction_map[new_edge.m_next_edge];
				new_edges.emplace_back(new_edge);
			}
		}
		for (size_t i = 0; i < _existing_vertices.size(); i++)
		{
			if (_existing_vertices[i])
			{
				new_vertices.emplace_back(_ch.m_vertices[i]);
			}
		}

		_ch.m_edges = std::move(new_edges);
		_ch.m_vertices = std::move(new_vertices);

		// Last pass to add vertices and edges to respective faces
		std::vector<bool> edges_processed(_ch.m_edges.size(), false);
		for (half_edge_idx edge = 0; edge < _ch.m_edges.size(); edge++)
		{
			half_edge_idx edge_iterator = edge;
			half_edge_data_structure::face& edge_face = _ch.m_faces[_ch.m_edges[edge_iterator].m_edge_face];
			while (!edges_processed[edge_iterator])
			{
				edge_face.m_edges.emplace_back(edge_iterator);
				edge_face.m_vertices.emplace_back(_ch.m_edges[edge_iterator].m_vertex);
				edges_processed[edge_iterator] = true;
				edge_iterator = get_next_index(edge_iterator);
			}
		}
	}

	half_edge_data_structure construct_half_edge_data_structure(
		glm::vec3 const* _vertices, size_t _vertex_count,
		glm::uvec3 const* _face_indices, size_t _face_count
	)
	{
		using half_edge = half_edge_data_structure::half_edge;
		using face = half_edge_data_structure::face;
		using half_edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::half_edge_idx;

		half_edge_data_structure new_hull;
		new_hull.m_vertices.resize(_vertex_count);
		memcpy(&new_hull.m_vertices.front(), _vertices, sizeof(glm::vec3) * _vertex_count);

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
				new_edges[e].m_next_edge = current_edge_count + (e + 1) % 3;
				new_edges[e].m_vertex = current_face_indices[e];
				new_edges[e].m_twin_edge = half_edge_data_structure::half_edge::INVALID_EDGE;
			}
			naive_edges.emplace_back(new_edges[0]);
			naive_edges.emplace_back(new_edges[1]);
			naive_edges.emplace_back(new_edges[2]);
		}

		pair_hds_twin_edges(naive_edges);

		new_hull.m_edges = std::move(naive_edges);

		// Indicates whether a face / an edge still exists.
		std::vector<bool> existing_faces(_face_count, true);
		std::vector<bool> existing_edges(new_hull.m_edges.size(), true);
		std::vector<bool> existing_vertices(new_hull.m_vertices.size(), false);
		auto const& vertices = new_hull.m_vertices; // Define shorthand

		// Optimization phase

		std::vector<glm::vec3> face_normals(_face_count);
		for (size_t i = 0; i < _face_count; i++)
		{
			glm::uvec3 face_indices = _face_indices[i];
			face_normals[i] = glm::normalize(glm::cross(
				new_hull.m_vertices[face_indices[1]] - new_hull.m_vertices[face_indices[0]],
				new_hull.m_vertices[face_indices[2]] - new_hull.m_vertices[face_indices[0]]
			));
		}

		merge_hds_features(
			new_hull,
			existing_vertices,
			existing_edges,
			existing_faces,
			face_normals
		);

		delete_unused_hds_features(
			new_hull,
			existing_vertices,
			existing_edges,
			existing_faces
		);

		return new_hull;
	}
}
}