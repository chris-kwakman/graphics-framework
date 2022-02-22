#include "convex_hull.h"

#include <Engine/Utils/singleton.h>
#include <Engine/Physics/point_hull.h>

#include <iostream>
#include <map>
#include <unordered_map>
#include <array>
#include <queue>
#include <stack>
#include <unordered_set>

#include <glm/gtc/epsilon.hpp>
#include <glm/geometric.hpp>



namespace Engine {
namespace Physics {


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
		half_edge_data_structure::face_idx face;
		half_edge_data_structure::vertex_idx vertex;
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
	* @param	half_edge_data_structure &							Hull that this will be generated for.
	* @param	unordered_map<face_idx,vertex_idx>		Output conflict list
	* @param	vector<vertex_idx>						Vertices to process
	*													If empty, process all vertices.
	* @param	vector<face_idx>						Faces to process
	*													If empty, process all faces.
	*/
	conflict_queue create_hull_conflict_lists(
		half_edge_data_structure& _ch,
		std::unordered_map<half_edge_data_structure::face_idx, std::vector<half_edge_data_structure::vertex_idx>> & _out_conflict_list,
		std::vector<bool> & _existing_vertices,
		std::vector<half_edge_data_structure::vertex_idx>  _process_vertices = {},
		std::vector<half_edge_data_structure::face_idx> _process_faces = {}
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

		std::vector<std::pair<half_edge_data_structure::face_idx, float>> maximal_dots(_process_vertices.size(), { -1, -std::numeric_limits<float>::max() });

		for (half_edge_data_structure::face_idx const current_face_idx : _process_faces)
		{
			half_edge_data_structure::face& current_face = _ch.m_faces[current_face_idx];
			glm::vec3 const curr_face_normal = glm::normalize(_ch.compute_face_normal(current_face_idx));
			// Arbitrary point on plane spanned by face
			glm::vec3 const curr_face_plane_point = _ch.m_vertices[current_face.m_vertices.front()];

			for (size_t i = 0; i < _process_vertices.size(); ++i)
			{
				half_edge_data_structure::vertex_idx const process_vtx_idx = _process_vertices[i];
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

		std::unordered_map<half_edge_data_structure::face_idx, std::pair<half_edge_data_structure::vertex_idx, float>> track_face_maximal_conflicts;
		for (half_edge_data_structure::face_idx const face_idx : _process_faces)
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

	half_edge_data_structure construct_convex_hull(glm::vec3 const* _vertices, size_t const _vertex_count, size_t _debug_iterations)
	{
		if (_vertex_count < 4)
			return half_edge_data_structure();

		using vertex_idx = half_edge_data_structure::vertex_idx;
		using edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::face_idx;
		using edge = half_edge_data_structure::half_edge;
		using face = half_edge_data_structure::face;

		half_edge_data_structure hull;
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

			pair_hds_twin_edges(hull.m_edges);

			for (half_edge_data_structure::vertex_idx const vertex_idx : tetrahedron_indices)
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
			glm::vec3 const cf_normal = hull.compute_face_normal(ci.face);

			// Start at arbitrary edge of conflict face.
			struct face_frame
			{
				edge_idx face_first_edge;
				edge_idx face_iter_edge;
			};

			edge_idx const INVALID_EDGE = half_edge_data_structure::half_edge::INVALID_EDGE;

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
						glm::vec3 const twin_face_normal = glm::normalize(hull.compute_face_normal(hull.m_edges[twin_edge].m_edge_face));
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
			pair_hds_twin_edges(hull.m_edges, new_edges.size(), &new_edges.front());

			// Mark faces used to determine horizon edges as deleted, as well as their edges.
			for (half_edge_data_structure::face_idx const delete_face_idx : visited_faces)
			{
				existing_faces[delete_face_idx] = false;

				for (half_edge_data_structure::half_edge_idx const delete_edge_idx : hull.m_faces[delete_face_idx].m_edges)
					existing_edges[delete_edge_idx] = false;
			}

			{
				// Collect vertices from conflict lists of horizon faces.
				// Only keep unique vertices.
				std::unordered_set<half_edge_data_structure::vertex_idx> horizon_face_conflict_vertices;
				for (half_edge_data_structure::face_idx const horizon_face_idx : visited_faces)
				{
					horizon_face_conflict_vertices.insert(
						face_conflict_lists[horizon_face_idx].begin(),
						face_conflict_lists[horizon_face_idx].end()
					);
				}
				std::vector<half_edge_data_structure::vertex_idx> process_vertices;
				process_vertices.reserve(horizon_face_conflict_vertices.size());
				std::vector<half_edge_data_structure::face_idx> process_faces;
				process_faces.reserve(visited_faces.size());
				
				for (half_edge_data_structure::vertex_idx const vtx_idx : horizon_face_conflict_vertices)
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

		merge_hds_features(
			hull,
			existing_vertices,
			existing_edges,
			existing_faces,
			face_normals
		);

		delete_unused_hds_features(
			hull,
			existing_vertices,
			existing_edges,
			existing_faces
		);

		return hull;
	}

	half_edge_data_structure construct_convex_hull(uint32_t _point_hull_handle, size_t const _debug_iterations)
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

	convex_hull_handle ConvexHullManager::RegisterConvexHull(half_edge_data_structure&& _hull, std::string _name)
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

}
}