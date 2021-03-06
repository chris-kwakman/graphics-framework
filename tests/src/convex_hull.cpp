#include <gtest/gtest.h>
#include <Engine/Physics/convex_hull.h>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace Engine::Physics;

void test_convex_hull_loops(half_edge_data_structure const& _hull, size_t _max_loop_length = 1024)
{
	std::vector<bool> processed_edges(_hull.m_edges.size(), false);

	for (half_edge_data_structure::half_edge_idx process_edge_idx = 0; process_edge_idx < _hull.m_edges.size(); ++process_edge_idx)
	{
		// Skip edge if we have already stepped through it.
		if (processed_edges[process_edge_idx])
			continue;
		processed_edges[process_edge_idx] = true;

		half_edge_data_structure::half_edge_idx edge_idx_iterator = process_edge_idx;
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

void test_face_vertices(half_edge_data_structure const& _hull)
{
	for (half_edge_data_structure::face_idx face_index = 0; face_index < _hull.m_faces.size(); face_index++)
	{
		auto const & face_vertex_indices = _hull.m_faces[face_index].m_vertices;

		glm::vec3 average_point(0.0f);
		for (half_edge_data_structure::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
			average_point += _hull.m_vertices[face_vertex_indices[vtx_index_iterator]];
		average_point /= (float)face_vertex_indices.size();

		//// Projection Method (check if distance from plane for each vertex is ~= 0)
		//glm::vec3 const n = glm::normalize(glm::cross(
		//	average_point - _hull.m_vertices[face_vertex_indices[0]], 
		//	average_point - _hull.m_vertices[face_vertex_indices[1]])
		//);
		//// Test if all vertices of face belong to same plane.
		//for (half_edge_data_structure::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
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
		 glm::vec3 const t = glm::normalize(_hull.m_vertices[face_vertex_indices[0]] - average_point);
		 glm::vec3 const n = glm::normalize(glm::cross(t, _hull.m_vertices[face_vertex_indices[1]] - average_point));
		 glm::vec3 const b = glm::normalize(glm::cross(n, t));
		 
		 EXPECT_FLOAT_EQ(glm::dot(t, n), 0.0f);
		 EXPECT_FLOAT_EQ(glm::dot(b, n), 0.0f);
		 
		 // Inverse of orthonormal matrix is transpose.
		 glm::mat3 const mat = glm::transpose(glm::mat3(t, b, n));
		 
		 std::vector<glm::vec2> vtx_uv_coordinates(face_vertex_indices.size());
		 // Test if all vertices of face belong to same plane.
		 for (half_edge_data_structure::vertex_idx vtx_index_iterator = 0; vtx_index_iterator < face_vertex_indices.size(); ++vtx_index_iterator)
		 {
		 	glm::vec3 const coordinates = mat * (_hull.m_vertices[face_vertex_indices[vtx_index_iterator]] - average_point);
			vtx_uv_coordinates[vtx_index_iterator] = glm::vec2(coordinates.x, coordinates.y);
		 	EXPECT_FLOAT_EQ(coordinates.z, 0.0f);
		 }

		 // Check if vertices are ordered CCW.
		 // Use trick from https://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order
		 float accumulator = 0.0f;
		 for (int i = 0; i < face_vertex_indices.size(); ++i)
		 {
			 glm::vec2 const v1 = vtx_uv_coordinates[i], v2 = vtx_uv_coordinates[(i+1)%vtx_uv_coordinates.size()];
			 accumulator += (v2.x - v1.x) * (v2.y + v1.y);
		 }
		 EXPECT_LE(accumulator, 0.0f);
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

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices,				sizeof(vertices) / sizeof(glm::vec3), 
		face_vertex_indices,	sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_vertices.size(), 3);
	EXPECT_EQ(new_hull.m_faces.size(), 1);
	EXPECT_EQ(new_hull.m_edges.size(), 3);

	test_convex_hull_loops(new_hull, 3);
	test_face_vertices(new_hull);

	EXPECT_EQ(new_hull.m_vertices.size(), 3);
	EXPECT_EQ(new_hull.m_edges.size(), 3);
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

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
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

TEST(ConvexHull, CubeConstruction)
{
	glm::vec3 const vertices[] = {
		glm::vec3(-1,-1,-1),
		glm::vec3(-1, -1,1),
		glm::vec3(-1, 1, 1),
		glm::vec3(-1, 1, -1),
		glm::vec3(1,-1,-1),
		glm::vec3(1, -1,1),
		glm::vec3(1, 1, 1),
		glm::vec3(1, 1, -1)
	};
	glm::uvec3 const face_vertex_indices[] = {
		glm::uvec3(0, 1, 2),
		glm::uvec3(0, 2, 3),
		glm::uvec3(5, 4, 6),
		glm::uvec3(7, 6, 4),
		glm::uvec3(1,5,6),
		glm::uvec3(1,6,2),
		glm::uvec3(4,0,3),
		glm::uvec3(4,3,7),
		glm::uvec3(0,5,1),
		glm::uvec3(0,4,5),
		glm::uvec3(3,2,6),
		glm::uvec3(3,6,7)
	};

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_edges.size(), 4 * 6);
	EXPECT_EQ(new_hull.m_faces.size(), 6);
	EXPECT_EQ(new_hull.m_vertices.size(), 8);

	test_convex_hull_loops(new_hull, 4);
	test_face_vertices(new_hull);
}

TEST(ConvexHull, DiscConstruction)
{
	unsigned int const RESOLUTION = 12;
	float const RADIUS = 1.0f;
	std::vector<glm::vec3> vertices;
	std::vector<glm::uvec3> face_vertex_indices;

	vertices.emplace_back(glm::vec3(0.0f));
	for (unsigned int i = 0; i < RESOLUTION; ++i)
	{
		float const rads = (float(i) / float(RESOLUTION)) * 2.0f * 3.1415f;
		vertices.emplace_back(cosf(rads) * RADIUS, 0.0f, sinf(rads) * RADIUS);
	}
	for (unsigned int i = 1; i <= RESOLUTION; ++i)
	{
		face_vertex_indices.emplace_back(0, i, (i % 12) + 1);
	}

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		&vertices.front(), vertices.size(),
		&face_vertex_indices.front(), face_vertex_indices.size()
	);

	EXPECT_EQ(new_hull.m_edges.size(), RESOLUTION);
	EXPECT_EQ(new_hull.m_faces.size(), 1);
	EXPECT_EQ(new_hull.m_vertices.size(), RESOLUTION);

	test_convex_hull_loops(new_hull, RESOLUTION);
	test_face_vertices(new_hull);
}

TEST(ConvexHull, LongSharedEdge)
{
	glm::vec3 const vertices[] = {
		glm::vec3(-1.0f, -1.0f, 0.0f),
		glm::vec3(1.0f, -1.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(-1.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 3.0f, 0.0f),
	};

	glm::uvec3 const face_vertex_indices[] =
	{
		glm::uvec3(0,3,4),
		glm::uvec3(0,1,3),
		glm::uvec3(1,2,3),
		glm::uvec3(4,3,5),
		glm::uvec3(3,2,5)
	};

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_edges.size(), 5);
	EXPECT_EQ(new_hull.m_faces.size(), 1);
	EXPECT_LE(new_hull.m_vertices.size(), 6);

	test_convex_hull_loops(new_hull, 5);
	test_face_vertices(new_hull);
}

TEST(ConvexHull, MergeColinearEdges)
{
	glm::vec3 const vertices[] = {
		glm::vec3(-1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	};

	glm::uvec3 const face_vertex_indices[] = {
		glm::uvec3(0,1,3),
		glm::uvec3(1,2,3)
	};

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 1);
	EXPECT_EQ(new_hull.m_edges.size(), 3);
	EXPECT_EQ(new_hull.m_vertices.size(), 3);
}

TEST(ConvexHull, MergeColinearEdges2)
{
	using v3 = glm::vec3;
	using uv3 = glm::uvec3;
	glm::vec3 const vertices[] = {
		v3(-1,-1,0),
		v3(1,-1,0),
		v3(1,0,0),
		v3(0,0,0),
		v3(-1,0,0),
		v3(-1,1,1),
		v3(0,1,1),
		v3(1,1,1)
	};
	glm::uvec3 const face_vertex_indices[] = {
		uv3(0,1,2),
		uv3(0,2,3),
		uv3(0,3,4),
		uv3(4,3,6),
		uv3(4,6,5),
		uv3(3,2,7),
		uv3(3,7,6)
	};

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 2);
	EXPECT_EQ(new_hull.m_edges.size(), 8);
	EXPECT_EQ(new_hull.m_vertices.size(), 6);
}

TEST(ConvexHull, MergeColinearEdges3)
{
	using v3 = glm::vec3;
	using uv3 = glm::uvec3;
	glm::vec3 const vertices[] = {
		v3(0,-1,0),
		v3(1,-1,0),
		v3(1,0,0),
		v3(0,0,0),
		v3(-1,0,0),
		v3(-1,1,1),
		v3(0,1,1),
		v3(1,1,1)
	};
	glm::uvec3 const face_vertex_indices[] = {
		uv3(0,1,2),
		uv3(0,2,3),
		uv3(4,3,6),
		uv3(4,6,5),
		uv3(3,2,7),
		uv3(3,7,6)
	};

	half_edge_data_structure new_hull = construct_half_edge_data_structure(
		vertices, sizeof(vertices) / sizeof(glm::vec3),
		face_vertex_indices, sizeof(face_vertex_indices) / sizeof(glm::uvec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 2);
	EXPECT_EQ(new_hull.m_edges.size(), 9);
	EXPECT_EQ(new_hull.m_vertices.size(), 7);
}

TEST(PointHull, Tetrahedron)
{
	using v3 = glm::vec3;
	v3 const vertices[] = {
		v3(0,0,0),
		v3(1,0,0),
		v3(0,1,0),
		v3(0,0,1)
	};

	half_edge_data_structure new_hull = construct_convex_hull(
		vertices, sizeof(vertices) / sizeof(glm::vec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 4);
	EXPECT_EQ(new_hull.m_edges.size(), 12);
	EXPECT_EQ(new_hull.m_vertices.size(), 4);
}

TEST(PointHull, Cube)
{
	using v3 = glm::vec3;
	v3 const vertices[] = {
		v3(0,0,0),
		v3(1,0,0),
		v3(0,1,0),
		v3(0,0,1),
		v3(1,1,1),
		v3(0,1,1),
		v3(1,0,1),
		v3(1,1,0)
	};

	half_edge_data_structure new_hull = construct_convex_hull(
		vertices, sizeof(vertices) / sizeof(glm::vec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 6);
	EXPECT_EQ(new_hull.m_edges.size(), 4*6);
	EXPECT_EQ(new_hull.m_vertices.size(), 8);
}

TEST(PointHull, Cube2)
{
	using v3 = glm::vec3;
	v3 const vertices[] = {
		v3(0,0,0),
		v3(1,0,0),
		v3(0,1,0),
		v3(0,0,1),
		v3(1,1,1),
		v3(0,1,1),
		v3(1,0,1),
		v3(1,1,0),
		v3(0.5)
	};

	half_edge_data_structure new_hull = construct_convex_hull(
		vertices, sizeof(vertices) / sizeof(glm::vec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 6);
	EXPECT_EQ(new_hull.m_edges.size(), 4 * 6);
	EXPECT_EQ(new_hull.m_vertices.size(), 8);
}

TEST(PointHull, Diamond)
{
	using v3 = glm::vec3;
	v3 const vertices[] = {
		v3(-1,-1,-1),
		v3(1,-1,-1),
		v3(-1,1,-1),
		v3(-1,-1,1),
		v3(1,1,1),
		v3(-1,1,1),
		v3(1,-1,1),
		v3(1,1,-1),
		v3(2,0,0),
		v3(-2,0,0),
		v3(0,2,0),
		v3(0,-2,0),
		v3(0,0,2),
		v3(0,0,-2),
	};

	half_edge_data_structure new_hull = construct_convex_hull(
		vertices, sizeof(vertices) / sizeof(glm::vec3)
	);

	EXPECT_EQ(new_hull.m_faces.size(), 8);
	EXPECT_EQ(new_hull.m_edges.size(), 3*8);
	EXPECT_EQ(new_hull.m_vertices.size(), 6);
}