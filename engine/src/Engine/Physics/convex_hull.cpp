#include "convex_hull.h"
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

#include <Engine/Utils/singleton.h>
#include <Engine/Physics/point_hull.h>

#include <unordered_map>
#include <algorithm>
#include <deque>
#include <array>
#include <queue>
#include <stack>
#include <unordered_set>

#include <iostream>

#include <imgui.h>

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

	/*
	* @brief	For every edge in the convex hull, set its twin edge (if it exists)
	* @param	std::vector<convex_hull::half_edge> &		List of edges to perform operation on.
	* @param	size_t				Number of specified edge indices to process
	* @param	half_edge_idx[]		Array of edge indices to perform operation on.
	* @details	If array is nullptr, perform on all indices.
	*/
	void compute_and_set_twin_edges(
		std::vector<convex_hull::half_edge> & _ch_edges,
		size_t _process_edge_count = 0, 
		convex_hull::half_edge_idx const* _process_edges = nullptr
	)
	{
		// Helper for creating vertex-pair map.
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

		/*
		Find twin edges
		*/

		std::map<vertex_pair, std::vector<convex_hull::half_edge_idx>> vertex_edge_map;

		// First pass to put edges belonging to same vertices in vectors belonging to those vertices.
		if (_process_edges)
		{
			for (size_t i = 0; i < _process_edge_count; i++)
			{
				convex_hull::half_edge_idx const e_idx = _process_edges[i];
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

	void merge_convex_hull_features(
		convex_hull& _ch,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces,
		std::vector<glm::vec3> const & _face_normals
	)
	{
		using half_edge = convex_hull::half_edge;
		using face = convex_hull::face;
		using half_edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::half_edge_idx;

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
			convex_hull::half_edge_idx const twin_next_edge = get_next_index(get_twin_index(edges_sharing_face.front()));
			convex_hull::half_edge_idx edge_iter = twin_next_edge;
			_ch.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
			// Iterate over twin edge face until we find edge leading to current edge vertex
			while (_ch.m_edges[edge_iter].m_next_edge != _ch.m_edges[edges_sharing_face.back()].m_twin_edge)
			{
				edge_iter = _ch.m_edges[edge_iter].m_next_edge;
				_ch.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
			}

			// Get edge previous to sequence of edges sharing face
			convex_hull::half_edge_idx previous_edge_idx = get_previous_edge(_ch, edges_sharing_face.front());
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

	void delete_unused_convex_hull_features(
		convex_hull& _ch,
		std::vector<bool>& _existing_vertices,
		std::vector<bool>& _existing_edges,
		std::vector<bool>& _existing_faces
	)
	{
		using half_edge = convex_hull::half_edge;
		using face = convex_hull::face;
		using half_edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::half_edge_idx;

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
				if (new_edge.m_twin_edge != convex_hull::half_edge::INVALID_EDGE)
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
			convex_hull::face& edge_face = _ch.m_faces[_ch.m_edges[edge_iterator].m_edge_face];
			while (!edges_processed[edge_iterator])
			{
				edge_face.m_edges.emplace_back(edge_iterator);
				edge_face.m_vertices.emplace_back(_ch.m_edges[edge_iterator].m_vertex);
				edges_processed[edge_iterator] = true;
				edge_iterator = get_next_index(edge_iterator);
			}
		}
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

		convex_hull new_hull;
		new_hull.m_vertices.resize(_vertex_count);
		memcpy(&new_hull.m_vertices.front(), _vertices, sizeof(glm::vec3)* _vertex_count);

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

		compute_and_set_twin_edges(naive_edges);

		new_hull.m_edges = std::move(naive_edges);

		// Indicates whether a face / an edge still exists.
		std::vector<bool> existing_faces(_face_count, true);
		std::vector<bool> existing_edges(new_hull.m_edges.size(), true);
		std::vector<bool> existing_vertices(new_hull.m_vertices.size(), false);
		auto const& vertices = new_hull.m_vertices; // Define shorthand

		// Convex hull optimization phase

		std::vector<glm::vec3> face_normals(_face_count);
		for (size_t i = 0; i < _face_count; i++)
		{
			glm::uvec3 face_indices = _face_indices[i];
			face_normals[i] = glm::normalize(glm::cross(
				new_hull.m_vertices[face_indices[1]] - new_hull.m_vertices[face_indices[0]],
				new_hull.m_vertices[face_indices[2]] - new_hull.m_vertices[face_indices[0]]
			));
		}

		merge_convex_hull_features(
			new_hull,
			existing_vertices,
			existing_edges,
			existing_faces,
			face_normals
		);

		delete_unused_convex_hull_features(
			new_hull,
			existing_vertices,
			existing_edges,
			existing_faces
		);

		return new_hull;
	}

	/*
	* @brief	Project a point onto a given plane
	* @param	glm::vec3	Plane point
	* @param	glm::vec3	Plane normal (NORMALIZED)
	* @param	glm::vec3	Point to project
	* @return	glm::vec3	Projected point.
	*/
	glm::vec3 project_point_to_plane(glm::vec3 _plane_point, glm::vec3 _plane_normal, glm::vec3 _point)
	{
		assert(glm::epsilonEqual(glm::dot(_plane_normal, _plane_normal), 1.0f, 0.00001f));
		return _point - glm::dot(_plane_normal, _point - _plane_point) * _plane_normal;
	};

	struct conflict_info
	{
		convex_hull::face_idx face;
		convex_hull::vertex_idx vertex;
		float dot;
		bool operator<(conflict_info const& l) const
		{
			return dot < l.dot;
		}
	};

	// Create conflict queue that sorts entries by largest dot values.
	using conflict_queue = std::priority_queue <
		conflict_info,
		std::vector<conflict_info>,
		std::less<conflict_info>
	>;

	/*
	* @brief	Merge two conflict queues
	* @param	conflict_queue &	Destination queue
	* @param	conflict_queue &	Source queue
	*/
	// Use https://stackoverflow.com/questions/15852355/how-to-merge-two-priority-queue
	void merge_conflict_queues(conflict_queue& _destination, conflict_queue & _source)
	{
		if (_destination.size() < _source.size()) {
			std::swap(_destination, _source);
		}
		while (!_source.empty()) {
			_destination.push(_source.top());
			_source.pop();
		}
	}

	/*
	* Helper function fo rcreating conflict list for a given set of faces to a given set of vertices
	* @param	convex_hull &							Hull that this will be generated for.
	* @param	unordered_map<face_idx,vertex_idx>		Output conflict list
	* @param	vector<vertex_idx>						Vertices to process
	*													If empty, process all vertices.
	* @param	vector<face_idx>						Faces to process
	*													If empty, process all faces.
	*/
	conflict_queue create_hull_conflict_lists(
		convex_hull& _ch,
		std::unordered_map<convex_hull::face_idx, std::vector<convex_hull::vertex_idx>> & _out_conflict_list,
		std::vector<bool> & _existing_vertices,
		std::vector<convex_hull::vertex_idx>  _process_vertices = {},
		std::vector<convex_hull::face_idx> _process_faces = {}
	)
	{
		if (_process_vertices.empty())
		{
			_process_vertices.resize(_ch.m_vertices.size());
			for (size_t i = 0; i < _process_vertices.size(); i++)
				_process_vertices[i] = i;
		}
		if (_process_faces.empty())
		{
			_process_faces.resize(_ch.m_faces.size());
			for (size_t i = 0; i < _process_faces.size(); i++)
				_process_faces[i] = i;
		}

		conflict_queue c_queue;

		std::vector<std::pair<convex_hull::face_idx, float>> maximal_dots(_process_vertices.size(), { -1, -std::numeric_limits<float>::max() });

		for (convex_hull::face_idx const current_face_idx : _process_faces)
		{
			convex_hull::face& current_face = _ch.m_faces[current_face_idx];
			glm::vec3 const curr_face_normal = glm::normalize(_ch.compute_normal(current_face_idx));
			// Arbitrary point on plane spanned by face
			glm::vec3 const curr_face_plane_point = _ch.m_vertices[current_face.m_vertices.front()];

			for (size_t i = 0; i < _process_vertices.size(); ++i)
			{
				convex_hull::vertex_idx const process_vtx_idx = _process_vertices[i];
				glm::vec3 const point = _ch.m_vertices[process_vtx_idx];
				glm::vec3 const proj_point = project_point_to_plane(curr_face_plane_point, curr_face_normal, point);
				float const dot = glm::dot(curr_face_normal, point - proj_point);
				// TODO: Consider fat planes.
				if (dot > 0.0f)
				{
					// Keep track of all vertices that are visible to the current face.
					// This will be used for re-partitioning vertices
					_out_conflict_list[current_face_idx].emplace_back(process_vtx_idx);

					// Keep track of maximal dot and the corresponding face.
					if (dot > maximal_dots[i].second)
						maximal_dots[i] = std::pair(current_face_idx, dot);
				}
			}
		}

		// Rather than pushing all conflicts of every face to the output conflict queue,
		// restrict it to the maximum conflicts of each face.

		std::unordered_map<convex_hull::face_idx, std::pair<convex_hull::vertex_idx, float>> track_face_maximal_conflicts;
		for (convex_hull::face_idx const face_idx : _process_faces)
			track_face_maximal_conflicts.emplace(face_idx, std::pair( -1, -std::numeric_limits<float>::max() ));

		for (size_t i = 0; i < _process_vertices.size(); i++)
		{
			// Mark vertices that are inside the hull for deletion.
			if (maximal_dots[i].second <= 0.0f)
			{
				_existing_vertices[_process_vertices[i]] = false;
			}
			else
			{
				// Keep track of greatest conflicts with each face.
				auto & face_ctracker = track_face_maximal_conflicts[maximal_dots[i].first];
				if (maximal_dots[i].second > face_ctracker.second)
				{
					face_ctracker.first = _process_vertices[i];
					face_ctracker.second = maximal_dots[i].second;
				}
			}
		}
		for (auto const & pair : track_face_maximal_conflicts)
		{
			// Only push to conflict queue if there is a conflicting vertex in front of the face.
			if (pair.second.second > 0.0f)
			{
				conflict_info c_info;
				c_info.face = pair.first;
				c_info.vertex = pair.second.first;
				c_info.dot = pair.second.second;
				c_queue.push(c_info);
			}
		}

		return c_queue;
	}

	convex_hull construct_convex_hull(glm::vec3 const* _vertices, size_t const _vertex_count, size_t _debug_iterations)
	{
		if (_vertex_count < 4)
			return convex_hull();

		using vertex_idx = convex_hull::vertex_idx;
		using edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::face_idx;
		using edge = convex_hull::half_edge;
		using face = convex_hull::face;

		convex_hull hull;
		hull.m_vertices.resize(_vertex_count);
		for (size_t i = 0; i < _vertex_count; i++)
			hull.m_vertices[i] = _vertices[i];

		std::vector<bool> existing_vertices(_vertex_count, true); // Stays the same size.
		std::vector<bool> existing_edges(12, true); // Mark all initial tetrahedron edges as existing.
		std::vector<bool> existing_faces(4, true); // Dynamically resizes

		std::array<vertex_idx, 4> tetrahedron_indices;

		// Create half-edge structure from an initial tetrahedron.
		// Pick this tetrahedron using a random triangle and an extreme point from this triangle.
		{
			//TODO: Create largest possible initial tetrahedron rather than taking a
			// random triangle and picking the furthest vertex.

			//// Get vertices in maximum directions along all axes.
			//struct axis_max_vertex
			//{
			//	float		abs_distance = -std::numeric_limits<float>::max();
			//	vertex_idx	vertex = -1;
			//};
			//axis_max_vertex maximal_vertices[6];
			//for (vertex_idx v = 0; v < _vertex_count; v++)
			//{
			//	for (int i = 0; i < 6; i++)
			//	{
			//		float sign = float(-1+2*(i % 2));
			//		if (float dist = _vertices[v][i / 2]; sign * dist > maximal_vertices[i].abs_distance)
			//		{
			//			maximal_vertices[i].abs_distance = sign * dist;
			//			maximal_vertices[i].vertex = v;
			//		}
			//	}
			//}
			//int max_axis = 0;
			//float max_dist = 0.0f;
			//for (size_t i = 0; i < 3; i++)
			//{
			//	float new_dist = maximal_vertices[2 * i + 1].abs_distance - maximal_vertices[2 * i].abs_distance;
			//	if (new_dist > max_dist)
			//	{
			//		max_axis = i;
			//		max_dist = new_dist;
			//	}
			//}


			glm::vec3 const init_triangle[3] = { _vertices[0], _vertices[1], _vertices[2] };
			glm::vec3 const init_tri_normal = glm::cross(_vertices[1] - _vertices[0], _vertices[2] - _vertices[0]);
			vertex_idx extreme_vtx_idx = -1;
			float max_dot = 0.0f;
			float max_abs_dot = 0.0f;

			for (size_t i = 3; i < _vertex_count; i++)
			{
				if (float dot = glm::dot(_vertices[i] - _vertices[0], init_tri_normal); std::abs(dot) > max_abs_dot)
				{
					max_dot = dot;
					max_abs_dot = std::abs(dot);
					extreme_vtx_idx = i;
				}
			}
			tetrahedron_indices = { 0,1,2,extreme_vtx_idx };

			auto add_tetrahedron_face_macro = [&](vertex_idx v0, vertex_idx v1, vertex_idx v2)
			{
				vertex_idx const input_vertices[3] = { v0,v1,v2 };
				face new_face;
				new_face.m_vertices = { tetrahedron_indices[v0], tetrahedron_indices[v1], tetrahedron_indices[v2] };
				edge new_edges[3];
				size_t const edge_count = hull.m_edges.size();
				for (size_t e = 0; e < 3; e++)
				{
					new_edges[e].m_edge_face = hull.m_faces.size();
					new_edges[e].m_next_edge = edge_count + (e+1)%3;
					new_edges[e].m_twin_edge = edge::INVALID_EDGE;
					new_edges[e].m_vertex = new_face.m_vertices[e];
					new_face.m_edges.emplace_back(hull.m_edges.size());
					hull.m_edges.emplace_back(std::move(new_edges[e]));
				}
				hull.m_faces.emplace_back(std::move(new_face));
			};

			// Create initial tetrahedron faces.
			// Pick an arbitrary triangle, and take the furthest point from it.
			// The order in which we define the vertices that form each face
			// depends on the sign of the maximal dot.
			if (max_dot > 0)
			{
				add_tetrahedron_face_macro(0, 2, 1);
				add_tetrahedron_face_macro(3, 2, 0);
				add_tetrahedron_face_macro(3, 1, 2);
				add_tetrahedron_face_macro(3, 0, 1);
			}
			else if (max_dot < 0)
			{
				add_tetrahedron_face_macro(0, 1, 2);
				add_tetrahedron_face_macro(3, 0, 2);
				add_tetrahedron_face_macro(3, 2, 1);
				add_tetrahedron_face_macro(3, 1, 0);
			}

			compute_and_set_twin_edges(hull.m_edges);

			for (convex_hull::vertex_idx const vertex_idx : tetrahedron_indices)
				existing_vertices[vertex_idx] = false;
		}

		// Map each face to a conflict list storing points that are visible to the face.
		std::unordered_map<face_idx, std::vector<vertex_idx>> face_conflict_lists;
		
		conflict_queue vertex_conflicts_queue = create_hull_conflict_lists(hull, face_conflict_lists, existing_vertices);

		// Resolve conflicts
		while (!vertex_conflicts_queue.empty() && _debug_iterations > 0)
		{
			conflict_info ci = vertex_conflicts_queue.top();
			vertex_conflicts_queue.pop();

			// Ignore conflict if its vertex is either discarded or its face is out of date
			// (i.e. vertex involved in conflict has been re-partitioned to a new face).
			if (!existing_vertices[ci.vertex] || !existing_faces[ci.face])
				continue;

			_debug_iterations -= 1;

			// Find the horizon of the current point
			// I.e. find the sequence of edges from which new faces to the current point must be made).
			// Depth-first search for edges whose twin edge's faces are not visible to this point.

			// Get normal of conflict face plane.
			glm::vec3 const cf_normal = hull.compute_normal(ci.face);

			// Start at arbitrary edge of conflict face.
			struct face_frame
			{
				edge_idx face_first_edge;
				edge_idx face_iter_edge;
			};

			edge_idx const INVALID_EDGE = convex_hull::half_edge::INVALID_EDGE;

			std::unordered_set<face_idx> visited_faces;
			std::vector<face_frame> frame_stack;
			face_frame first_frame;
			first_frame.face_first_edge = hull.m_faces[ci.face].m_edges.front();
			first_frame.face_iter_edge = first_frame.face_first_edge;
			frame_stack.push_back(first_frame);
			visited_faces.emplace(ci.face);

			std::vector<edge_idx> horizon_edges;

			do
			{
				face_frame& curr_face_frame = frame_stack.back();

				if (edge_idx const twin_edge = hull.m_edges[curr_face_frame.face_iter_edge].m_twin_edge; twin_edge != INVALID_EDGE)
				{
					if (visited_faces.find(hull.m_edges[twin_edge].m_edge_face) == visited_faces.end())
					{
						glm::vec3 const twin_face_normal = glm::normalize(hull.compute_normal(hull.m_edges[twin_edge].m_edge_face));
						// If twin face is not visible from current conflict point,
						// we have reached a horizon edge.
						glm::vec3 const point = hull.m_vertices[ci.vertex];
						glm::vec3 const proj_point = project_point_to_plane(hull.m_vertices[hull.m_edges[twin_edge].m_vertex], twin_face_normal, point);
						if (glm::dot(point - proj_point, twin_face_normal) < 0.0f)
						{
							horizon_edges.emplace_back(curr_face_frame.face_iter_edge);
						}
						// If twin face is visible, perform DFS and add twin face to stack
						else
						{
							face_frame new_frame;
							new_frame.face_first_edge = twin_edge;
							new_frame.face_iter_edge = twin_edge;
							visited_faces.emplace(hull.m_edges[twin_edge].m_edge_face);
							frame_stack.emplace_back(new_frame);
						}
					}
				}

				// Traverse along top frame's face edge list.
				frame_stack.back().face_iter_edge = hull.m_edges[frame_stack.back().face_iter_edge].m_next_edge;

				// If we have traversed all of this face's edges, pop the frame off the stack.
				if (frame_stack.back().face_iter_edge == frame_stack.back().face_first_edge)
					frame_stack.pop_back();
			} while (!frame_stack.empty());

			// Found horizon edges are in reverse order.

			std::vector<edge_idx> new_edges;
			std::vector<face_idx> new_faces;

			// Create triangle faces from horizon edges and conflict point.
			for (edge_idx const horizon_edge_idx : horizon_edges)
			{
				existing_edges[horizon_edge_idx] = false;

				face_idx const new_face_idx = hull.m_faces.size();
				edge_idx const new_edge_idx = hull.m_edges.size();
				face new_face;
				new_face.m_vertices.resize(3);
				new_face.m_vertices[0] = hull.m_edges[horizon_edge_idx].m_vertex;
				new_face.m_vertices[1] = hull.m_edges[hull.m_edges[horizon_edge_idx].m_next_edge].m_vertex;
				new_face.m_vertices[2] = ci.vertex;

				// Update horizon edge's twin to point to edge of new triangle.
				edge const horizon_edge = hull.m_edges[horizon_edge_idx];
				hull.m_edges[horizon_edge.m_twin_edge].m_twin_edge = new_edge_idx;

				// Create edges for new triangle.
				edge e0, e1, e2;
				e0.m_edge_face = new_face_idx;
				e0.m_twin_edge = INVALID_EDGE;
				e1 = e0;
				e2 = e0;
				e0.m_vertex = horizon_edge.m_vertex;
				e0.m_twin_edge = horizon_edge.m_twin_edge;
				e0.m_next_edge = new_edge_idx + 1;
				e1.m_vertex = new_face.m_vertices[1];
				e1.m_next_edge = new_edge_idx+2;
				e2.m_vertex = new_face.m_vertices[2];
				e2.m_next_edge = new_edge_idx;

				// Add edges to new face.
				new_face.m_edges = { new_edge_idx, edge_idx(new_edge_idx + 1u), edge_idx(new_edge_idx + 2u) };

				// Keep track of new edge indices for pairing twin edges later.
				new_edges.emplace_back(new_edge_idx);
				new_edges.emplace_back(edge_idx(new_edge_idx + 1u));
				new_edges.emplace_back(edge_idx(new_edge_idx + 2u));

				new_faces.emplace_back(new_face_idx);

				hull.m_edges.emplace_back(e0);
				hull.m_edges.emplace_back(e1);
				hull.m_edges.emplace_back(e2);
				hull.m_faces.emplace_back(new_face);

				existing_faces.emplace_back(true);

				existing_edges.emplace_back(true);
				existing_edges.emplace_back(true);
				existing_edges.emplace_back(true);
			}

			// Find and set twin edges of newly added triangles.
			compute_and_set_twin_edges(hull.m_edges, new_edges.size(), &new_edges.front());

			// Mark faces used to determine horizon edges as deleted, as well as their edges.
			for (convex_hull::face_idx const delete_face_idx : visited_faces)
			{
				existing_faces[delete_face_idx] = false;

				for (convex_hull::half_edge_idx const delete_edge_idx : hull.m_faces[delete_face_idx].m_edges)
					existing_edges[delete_edge_idx] = false;
			}

			{
				// Collect vertices from conflict lists of horizon faces.
				// Only keep unique vertices.
				std::unordered_set<convex_hull::vertex_idx> horizon_face_conflict_vertices;
				for (convex_hull::face_idx const horizon_face_idx : visited_faces)
				{
					horizon_face_conflict_vertices.insert(
						face_conflict_lists[horizon_face_idx].begin(),
						face_conflict_lists[horizon_face_idx].end()
					);
				}
				std::vector<convex_hull::vertex_idx> process_vertices;
				process_vertices.reserve(horizon_face_conflict_vertices.size());
				std::vector<convex_hull::face_idx> process_faces;
				process_faces.reserve(visited_faces.size());
				
				for (convex_hull::vertex_idx const vtx_idx : horizon_face_conflict_vertices)
					process_vertices.push_back(vtx_idx);

				merge_conflict_queues(
					vertex_conflicts_queue,
					create_hull_conflict_lists(
						hull,
						face_conflict_lists,
						existing_vertices,
						process_vertices,
						new_faces
					)
				);

			}
		}

		std::vector<glm::vec3> face_normals(hull.m_faces.size());
		for (size_t i = 0; i < hull.m_faces.size(); i++)
		{
			if (existing_faces[i])
			{
				auto const& face = hull.m_faces[i];
				face_normals[i] = glm::normalize(glm::cross(
					hull.m_vertices[face.m_vertices[1]] - hull.m_vertices[face.m_vertices[0]],
					hull.m_vertices[face.m_vertices[2]] - hull.m_vertices[face.m_vertices[0]]
				));
			}
		}

		merge_convex_hull_features(
			hull,
			existing_vertices,
			existing_edges,
			existing_faces,
			face_normals
		);

		delete_unused_convex_hull_features(
			hull,
			existing_vertices,
			existing_edges,
			existing_faces
		);

		return hull;
	}

	convex_hull construct_convex_hull(uint32_t _point_hull_handle, size_t const _debug_iterations)
	{
		point_hull const * hull = Singleton<PointHullManager>().GetPointHull(_point_hull_handle);
		assert(hull);
		return construct_convex_hull(&hull->m_points.front(), hull->m_points.size(), _debug_iterations);
	}





	ConvexHullManager::convex_hull_info const* ConvexHullManager::GetConvexHullInfo(convex_hull_handle _handle) const
	{
		auto iter = m_map.find(_handle);
		return (iter == m_map.end()) ? nullptr : &iter->second;
	}

	convex_hull_handle ConvexHullManager::RegisterConvexHull(convex_hull&& _hull, std::string _name)
	{
		convex_hull_info entry;
		entry.m_data = std::move(_hull);
		entry.m_name = _name;

		convex_hull_handle const new_handle = m_handle_counter++;
		m_map.emplace(new_handle, std::move(entry));

		return new_handle;
	}

	void ConvexHullManager::DeleteConvexHull(convex_hull_handle _handle)
	{
		m_map.erase(_handle);
	}

	void DisplayConvexHullDataDebug(convex_hull const* _hull)
	{
		static int set_scroll_idx_edges = -1, set_scroll_idx_faces = -1;

		ImGuiTableFlags const table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

		if (ImGui::BeginTable("Edges", 6, table_flags,
			glm::vec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeightWithSpacing() * 10.0f)
		))
		{
			ImGui::TableSetupColumn("Edge IDX", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Next Edge IDX");
			ImGui::TableSetupColumn("Twin Edge IDX");
			ImGui::TableSetupColumn("Face IDX");
			ImGui::TableSetupColumn("Vertex IDX");
			ImGui::TableSetupColumn("Vertex");
			ImGui::TableHeadersRow();

			for (size_t edge_idx = 0; edge_idx < _hull->m_edges.size(); edge_idx++)
			{
				convex_hull::half_edge const& edge = _hull->m_edges[edge_idx];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%u", edge_idx);
				ImGui::TableNextColumn();
				ImGui::Text("%u", edge.m_next_edge);
				if (ImGui::IsItemClicked())
					set_scroll_idx_edges = edge.m_next_edge;
				ImGui::TableNextColumn();
				ImGui::Text("%u", edge.m_twin_edge);
				if (ImGui::IsItemClicked())
					set_scroll_idx_edges = edge.m_twin_edge;
				ImGui::TableNextColumn();
				ImGui::Text("%u", edge.m_edge_face);
				if (ImGui::IsItemClicked())
					set_scroll_idx_faces = edge.m_edge_face;
				ImGui::TableNextColumn();
				ImGui::Text("%u", edge.m_vertex);
				ImGui::TableNextColumn();
				glm::vec3 const vtx = _hull->m_vertices[edge.m_vertex];
				ImGui::Text("%.3f,%.3f,%.3f", vtx.x, vtx.y, vtx.z);
				if(set_scroll_idx_edges == edge_idx)
				{
					set_scroll_idx_edges = -1;
					ImGui::SetScrollHereY(1.0f);
				}
			}
			ImGui::EndTable();
		}
		if (ImGui::BeginTable("Faces", 3, table_flags,
			glm::vec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeightWithSpacing() * 10.0f)
		))
		{
			ImGui::TableSetupColumn("Face IDX", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Edges");
			ImGui::TableSetupColumn("Vertices");
			ImGui::TableHeadersRow();

			for (size_t face_idx = 0; face_idx < _hull->m_faces.size(); face_idx++)
			{
				convex_hull::face const& face = _hull->m_faces[face_idx];

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%u", face_idx);
				ImGui::TableNextColumn();
				char buffer[4096];
				{
					size_t chars_written = 0;
					for (size_t i = 0; i < face.m_edges.size(); i++)
						chars_written += snprintf(buffer + chars_written, sizeof(buffer) - chars_written, "%u,", face.m_edges[i]);
					ImGui::TextUnformatted(buffer, buffer + chars_written);
				}
				ImGui::TableNextColumn();
				{
					size_t chars_written = 0;
					for (size_t i = 0; i < face.m_vertices.size(); i++)
						chars_written += snprintf(buffer + chars_written, sizeof(buffer) - chars_written, "%u,", face.m_vertices[i]);
					ImGui::TextUnformatted(buffer, buffer + chars_written);
				}
				if (set_scroll_idx_faces == face_idx)
				{
					set_scroll_idx_faces = -1;
					ImGui::SetScrollHereY(1.0f);
				}
			}
			ImGui::EndTable();
		}

	}

	/*
	* @brief	Compute the normal of a face belonging to this hull.
	* @param	face_idx		Index of face whose normal to compute.
	* @returns	glm::vec3		Face normal
	*/
	glm::vec3 convex_hull::compute_normal(face_idx _face) const
	{
		// Assume face vertices are ordered CCW and form a convex polygon.
		auto const& face = m_faces[_face];
		glm::vec3 const e1 = m_vertices[face.m_vertices[1]] - m_vertices[face.m_vertices[0]];
		glm::vec3 const e2 = m_vertices[face.m_vertices[2]] - m_vertices[face.m_vertices[1]];
		return glm::cross(e1, e2);
	}
}
}