#include <gtest/gtest.h>
#include <Engine/Physics/convex_hull.h>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace Engine::Physics;

void test_convex_hull_loops(convex_hull const& _hull, size_t _max_loop_length = 1024)
{
	std::vector<bool> processed_edges(_hull.m_edges.size(), false);

	for (convex_hull::half_edge_idx process_edge_idx = 0; process_edge_idx < _hull.m_edges.size(); ++process_edge_idx)
	{
		// Skip edge if we have already stepped through it.
		if (processed_edges[process_edge_idx])
			continue;
		processed_edges[process_edge_idx] = true;

		convex_hull::half_edge_idx edge_idx_iterator = process_edge_idx;
		size_t current_loop_length = 0;
		do
		{
			edge_idx_iterator = _hull.m_edges[edge_idx_iterator].m_next_edge;
			processed_edges[edge_idx_iterator] = true;
			current_loop_length += 1;
		} while (edge_idx_iterator != process_edge_idx && current_loop_length <= _max_loop_length);

		ASSERT_LE(current_loop_length, _max_loop_length);
	}
}

void test_face_vertices(convex_hull const& _hull)
{
	for (convex_hull::face_idx face_index = 0; face_index < _hull.m_faces.size(); face_index++)
	{
		auto const & face_vertex_indices = _hull.m_faces[face_index].m_vertices;

		glm::vec3 average_point(0.0f);
		for (convex_hull::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
			average_point += _hull.m_vertices[face_vertex_indices[vtx_index_iterator]];
		average_point /= (float)face_vertex_indices.size();

		//// Projection Method (check if distance from plane for each vertex is ~= 0)
		//glm::vec3 const n = glm::normalize(glm::cross(
		//	average_point - _hull.m_vertices[face_vertex_indices[0]], 
		//	average_point - _hull.m_vertices[face_vertex_indices[1]])
		//);
		//// Test if all vertices of face belong to same plane.
		//for (convex_hull::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
		//{
		//	ASSERT_TRUE(
		//		glm::epsilonEqual(
		//			glm::dot(n, average_point - _hull.m_vertices[face_vertex_indices[vtx_index_iterator]]), 
		//			0.0f, 
		//			0.0000001f
		//		)
		//	);
		//}

		// Orthonormal matrix method.
		// Use this over the previous method since it returns a UV coordinate that
		// we can use to check if vertices are defined CCW within face.
		// Orthonormal vectors
		 glm::vec3 const t = glm::normalize(average_point - _hull.m_vertices[face_vertex_indices[0]]);
		 glm::vec3 const n = glm::normalize(glm::cross(t, average_point - _hull.m_vertices[face_vertex_indices[1]]));
		 glm::vec3 const b = glm::normalize(glm::cross(n, t));
		 
		 ASSERT_TRUE(glm::epsilonEqual(glm::dot(t, n), 0.0f, 0.000000001f));
		 ASSERT_TRUE(glm::epsilonEqual(glm::dot(b, n), 0.0f, 0.000000001f));
		 
		 // Inverse of orthonormal matrix is transpose.
		 glm::mat3 const mat = glm::transpose(glm::mat3(t, b, n));
		 
		 std::vector<glm::vec2> vtx_uv_coordinates(face_vertex_indices.size());
		 // Test if all vertices of face belong to same plane.
		 for (convex_hull::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
		 {
		 	glm::vec3 const coordinates = mat * _hull.m_vertices[face_vertex_indices[vtx_index_iterator]];
			vtx_uv_coordinates[vtx_index_iterator] = glm::vec2(coordinates.x, coordinates.y);
		 	ASSERT_TRUE(glm::epsilonEqual(coordinates.z, 0.0f, 0.0000001f));
		 }

		 // Check if vertices are ordered CCW.
		 // Use trick from https://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order
		 float accumulator = 0.0f;
		 for (int i = 0; i < face_vertex_indices.size(); ++i)
		 {
			 glm::vec2 const v1 = vtx_uv_coordinates[i], v2 = vtx_uv_coordinates[(i+1)%vtx_uv_coordinates.size()];
			 accumulator += (v2.x - v1.x) * (v2.y + v1.y);
		 }
		 ASSERT_LT(accumulator, 0.0f);
	}
}

TEST(ConvexHull, TriangleConstruction)
{
	glm::vec3 const vertices[] = {
		glm::vec3(-1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f)
	};
	glm::uvec3 const face_vertex_indices[] = {
		glm::uvec3{0,1,2}
	};

	convex_hull new_hull = construct_convex_hull(
		vertices,				sizeof(vertices) / sizeof(glm::vec3), 
		face_vertex_indices,	sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	ASSERT_EQ(new_hull.m_vertices.size(), 3);
	ASSERT_EQ(new_hull.m_faces.size(), 1);
	ASSERT_EQ(new_hull.m_edges.size(), 3);

	test_convex_hull_loops(new_hull, 3);
	test_face_vertices(new_hull);
}

TEST(ConvexHull, SquareConstruction)
{
	glm::vec3 const vertices[] = {
		glm::vec3(-1.0f, -1.0f, 0.0f),
		glm::vec3(1.0f, -1.0f, 0.0f),
		glm::vec3(1.0f,1.0f, 0.0f),
		glm::vec3(-1.0f, 1.0f, 0.0f)
	};
	glm::uvec3 const face_vertex_indices[] = {
		glm::uvec3(0,1,2), glm::uvec3(0,2,3)
	};

	convex_hull new_hull = construct_convex_hull(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	// Before merging, this should be the expected result
	//ASSERT_EQ(new_hull.m_faces, 2);
	// After merging, this is the actual result
	ASSERT_EQ(new_hull.m_faces.size(), 1);
	ASSERT_EQ(new_hull.m_vertices.size(), 4);

	test_convex_hull_loops(new_hull, 4);
	test_face_vertices(new_hull);
}