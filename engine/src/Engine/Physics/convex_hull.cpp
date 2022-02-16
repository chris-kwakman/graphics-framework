#include "convex_hull.h"
#include <map>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

#include <Engine/Utils/singleton.h>
#include <Engine/Physics/point_hull.h>

#include <algorithm>
#include <deque>

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

		// Indicates whether a face / an edge still exists.
		std::vector<bool> existing_faces(_face_count, true);
		std::vector<bool> existing_edges(new_hull.m_edges.size(), true);
		std::vector<bool> existing_vertices(new_hull.m_vertices.size(), false);
		auto const& vertices = new_hull.m_vertices; // Define shorthand

		// Convex hull optimization phase
		if (true)
		{
			//// Face Merging

			// Second pass to merge coplanar faces.
			// The resulting face must be CONVEX.
			// Rather than checking for convexity, we assume
			// the input data will result in convex faces.

			// Pre-calculate face normals to make my life easier.
			// Assume face vertices are defined in CCW order!
			std::vector<glm::vec3> face_normals(_face_count);
			for (size_t i = 0; i < _face_count; ++i)
			{
				auto const& face_vtx_indices = _face_indices[i];
				face_normals[i] = glm::normalize(glm::cross(
					_vertices[face_vtx_indices[2]] - _vertices[face_vtx_indices[0]],
					_vertices[face_vtx_indices[1]] - _vertices[face_vtx_indices[0]]
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

					//// Traverse forwards along side of current edge.
					//half_edge_idx iterator_index = edge_index;
					//half_edge iterator_edge = new_hull.m_edges[iterator_index];
					//while (
					//	iterator_edge.m_twin_edge != half_edge::INVALID_EDGE &&
					//	get_face_index(iterator_edge.m_twin_edge) == curr_twin_edge.m_edge_face
					//	)
					//{
					//	edges_sharing_face.emplace_back(iterator_index);
					//	iterator_index = iterator_edge.m_next_edge;
					//	iterator_edge = new_hull.m_edges[iterator_index];
					//}
					//// Traverse backwards along twin side of current edge.
					//iterator_index = get_next_index(get_twin_index(edge_index));
					//iterator_edge = new_hull.m_edges[iterator_index];
					//while (
					//	iterator_edge.m_twin_edge != half_edge::INVALID_EDGE &&
					//	get_face_index(iterator_edge.m_twin_edge) == current_edge.m_edge_face
					//	)
					//{
					//	edges_sharing_face.emplace_front(iterator_edge.m_twin_edge);
					//	iterator_index = iterator_edge.m_next_edge;
					//	iterator_edge = new_hull.m_edges[iterator_index];
					//}
				}

				// Mark edges in sequence and their twins as deleted
				for (size_t i = 0; i < edges_sharing_face.size(); i++)
				{
					existing_edges[edges_sharing_face[i]] = false;
					existing_edges[get_twin_index(edges_sharing_face[i])] = false;
				}
				// Mark twin edge's face as deleted
				existing_faces[get_face_index(get_twin_index(edges_sharing_face.front()))] = false;

				// Insert all vertices past current edge's origin vertex until we find edge whose twin is the back element in the edge sequence.
				convex_hull::half_edge_idx const twin_next_edge = get_next_index(get_twin_index(edges_sharing_face.front()));
				convex_hull::half_edge_idx edge_iter = twin_next_edge;
				new_hull.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
				// Iterate over twin edge face until we find edge leading to current edge vertex
				while (new_hull.m_edges[edge_iter].m_next_edge != new_hull.m_edges[edges_sharing_face.back()].m_twin_edge)
				{
					edge_iter = new_hull.m_edges[edge_iter].m_next_edge;
					new_hull.m_edges[edge_iter].m_edge_face = current_edge.m_edge_face;
				}

				// Get edge previous to sequence of edges sharing face
				convex_hull::half_edge_idx previous_edge_idx = get_previous_edge(new_hull, edges_sharing_face.front());
				// Set each edge's previous edge's next edge to next edge of their corresponding twin edge.
				// (Very edgy)
				// AKA join loop formed by both face's edges, and remove edges between these two faces.
				new_hull.m_edges[previous_edge_idx].m_next_edge = get_next_index(get_twin_index(edges_sharing_face.front()));
				new_hull.m_edges[edge_iter].m_next_edge = new_hull.m_edges[edges_sharing_face.back()].m_next_edge;
			}

			//// Co-linear edge merging
			if (true)
			{

				bool edge_was_merged = false;

				// Pre-compute edge directions
				std::vector<glm::vec3> edge_normalized_dirs(new_hull.m_edges.size());
				for (half_edge_idx edge_idx = 0; edge_idx < new_hull.m_edges.size(); edge_idx++)
				{
					if (!existing_edges[edge_idx])
						continue;

					edge_normalized_dirs[edge_idx] = glm::normalize(
						vertices[new_hull.m_edges[get_next_index(edge_idx)].m_vertex] - vertices[new_hull.m_edges[edge_idx].m_vertex]
					);
				}

				// For each edge, check if the current and next edge each have the same face on their sides.
				// Do the same for their twin edges.
				for (half_edge_idx edge_idx = 0; edge_idx < new_hull.m_edges.size(); edge_idx++)
				{
					if (!existing_edges[edge_idx])
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
						existing_edges[next_idx] = false;
						new_hull.m_edges[edge_idx].m_next_edge = get_next_index(next_idx);
						edge_was_merged = true;
					}
				}
			}

			// Mark all vertices found in existing edges as existing.
			for (half_edge_idx edge_idx = 0; edge_idx < new_hull.m_edges.size(); edge_idx++)
			{
				if (!existing_edges[edge_idx])
					continue;

				existing_vertices[new_hull.m_edges[edge_idx].m_vertex] = true;
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

			std::vector<unsigned int> vertex_index_substraction_map(existing_vertices.size());
			vertex_index_substraction_map[0] = (unsigned int)!existing_vertices[0];
			for (size_t i = 1; i < vertex_index_substraction_map.size(); i++)
				vertex_index_substraction_map[i] = vertex_index_substraction_map[i - 1] + (unsigned int)!existing_vertices[i];

			// New edges & faces array where indices marked for deletion are erased.
			decltype(new_hull.m_edges) new_edges;
			new_edges.reserve(existing_edges.size() - edge_index_substraction_map.back());
			decltype(new_hull.m_vertices) new_vertices;
			new_vertices.reserve(existing_vertices.size() - vertex_index_substraction_map.back());
			new_hull.m_faces.resize(existing_faces.size() - face_index_substraction_map.back());

			for (size_t i = 0; i < existing_edges.size(); i++)
			{
				if (existing_edges[i])
				{
					auto new_edge = new_hull.m_edges[i];
					new_edge.m_edge_face -= face_index_substraction_map[new_edge.m_edge_face];
					new_edge.m_vertex -= vertex_index_substraction_map[new_edge.m_vertex];
					if (new_edge.m_twin_edge != convex_hull::half_edge::INVALID_EDGE)
						new_edge.m_twin_edge -= edge_index_substraction_map[new_edge.m_twin_edge];
					new_edge.m_next_edge -= edge_index_substraction_map[new_edge.m_next_edge];
					new_edges.emplace_back(new_edge);
				}
			}
			for (size_t i = 0; i < existing_vertices.size(); i++)
			{
				if (existing_vertices[i])
				{
					new_vertices.emplace_back(new_hull.m_vertices[i]);
				}
			}

			new_hull.m_edges = std::move(new_edges);
			new_hull.m_vertices = std::move(new_vertices);
		}
		else
		{
			new_hull.m_faces.resize(existing_faces.size());
		}

		// Last pass to add vertices and edges to respective faces
		std::vector<bool> edges_processed(new_hull.m_edges.size(), false);
		for (half_edge_idx edge = 0; edge < new_hull.m_edges.size(); edge++)
		{
			half_edge_idx edge_iterator = edge;
			face& edge_face = new_hull.m_faces[new_hull.m_edges[edge_iterator].m_edge_face];
			while (!edges_processed[edge_iterator])
			{
				edge_face.m_edges.emplace_back(edge_iterator);
				edge_face.m_vertices.emplace_back(new_hull.m_edges[edge_iterator].m_vertex);
				edges_processed[edge_iterator] = true;
				edge_iterator = get_next_index(edge_iterator);
			}
		}
		return new_hull;
	}

	convex_hull construct_convex_hull(glm::vec3 const* _vertices, size_t const _vertex_count, size_t const _debug_iterations)
	{
		if (_vertex_count < 4)
			return convex_hull();

		using vertex_idx = convex_hull::vertex_idx;
		using edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::face_idx;
		using edge = convex_hull::half_edge;
		using face = convex_hull::face;

		convex_hull hull;

		// Create half-edge structure from an initial tetrahedron.
		// Pick this tetrahedron using a random triangle and an extreme point from this triangle.
		{
			glm::vec3 const init_triangle[3] = { _vertices[0], _vertices[1], _vertices[2] };
			glm::vec3 const init_tri_normal = glm::cross(_vertices[1] - _vertices[0], _vertices[2] - _vertices[0]);
			vertex_idx extreme_vtx_idx = -1;
			float maximal_distance = 0.0f;

			for (size_t i = 3; i < _vertex_count; i++)
			{
				if (float dot = glm::dot(_vertices[i] - _vertices[0], init_tri_normal); dot > maximal_distance)
				{
					maximal_distance = dot;
					extreme_vtx_idx = i;
				}
			}

			vertex_idx const tetrahedron_indices[] = { 0,1,2,extreme_vtx_idx };
			hull.m_vertices = { _vertices[0], _vertices[1], _vertices[2], _vertices[extreme_vtx_idx] };

			// Create initial tetrahedron faces.
			for (face_idx i = 0; i < 4; i++)
			{
				face new_face;
				new_face.m_vertices = { tetrahedron_indices[i % 4], tetrahedron_indices[(i + 1) % 4],tetrahedron_indices[(i + 2) % 4] };
				
				edge new_edges[3];
				for (size_t e = 0; e < 3; e++)
				{
					new_edges[e].m_edge_face = i;
					new_edges[e].m_next_edge = hull.m_edges.size() + 1;
					new_edges[e].m_twin_edge = edge::INVALID_EDGE;
					new_edges[e].m_vertex = new_face.m_vertices[e];
				}
				new_face.m_edges = { (edge_idx)hull.m_edges.size(), (edge_idx)(hull.m_edges.size() + 1), (edge_idx)(hull.m_edges.size() + 2) };
				hull.m_edges.emplace_back(new_edges[0]);
				hull.m_edges.emplace_back(new_edges[1]);
				hull.m_edges.emplace_back(new_edges[2]);
				hull.m_faces.emplace_back(std::move(new_face));
			}
		}

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
}
}