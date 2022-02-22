#include "intersection.h"

namespace Engine {
using namespace Math;
namespace Physics {

	/*
	* Perform intersection test of ray with transformed convex hull.
	* @param	ray				NORMALIZED intersection ray
	* @param	convex_hull		Hull we want to perform test against
	* @param	transform3D		World transform of hull.
	* @details	Transforms ray into local space of convex hull object.
	*/
	intersection_result intersect_ray_half_edge_data_structure(
		ray _ray, half_edge_data_structure const & _hds,
 
		transform3D const& _hull_transform
	)
	{
		// Transform ray into local space of convex hull object.
		glm::mat2x4 const data = _hull_transform.GetInvMatrix() * glm::mat2x4(glm::vec4(_ray.origin, 1.0f), glm::vec4(_ray.dir, 0.0f));
		_ray.origin = data[0];
		_ray.dir = glm::normalize(data[1]);

		uint16_t const INVALID_FACE_INDEX = -1;
		intersection_result minimum_intersection_face{ std::numeric_limits<float>::max(), INVALID_FACE_INDEX, glm::vec3(0.0f)};
		glm::vec3 vertices_stack[256];
		for (half_edge_data_structure::face_idx face_index = 0; face_index < _hds.m_faces.size(); face_index++)
		{
			// Copy vertices to vector.
			size_t face_vertex_count = 0;
			for (size_t face_vtx_idx = 0; face_vtx_idx < _hds.m_faces[face_index].m_vertices.size(); face_vtx_idx++)
			{
				vertices_stack[face_vertex_count++] = _hds.m_vertices[_hds.m_faces[face_index].m_vertices[face_vtx_idx]];
			}
			intersection_result const face_result = intersect_ray_convex_polygon(_ray, vertices_stack, face_vertex_count);
			if (face_result.t > 0.0f && face_result.t < minimum_intersection_face.t /*&& glm::dot(face_result.normal, _ray.dir) < 0*/)
			{
				minimum_intersection_face = face_result;
				minimum_intersection_face.face_index = face_index;
			}
		}

		return minimum_intersection_face.face_index == INVALID_FACE_INDEX
			? intersection_result{ -1.0f, INVALID_FACE_INDEX, glm::vec3(0.0f) }
			: minimum_intersection_face;
	}

	/*
	* Perform intersection test of ray against convex polygon
	* @param	ray				NORMALIZED intersection ray
	* @param	glm::vec3 *		Pointer to array of convex polygon vertices, CCW order
	* @param	size_t			Size of vertex array
	* @details	Ray and vertices must be in the same space. Normal of polygon face
	*			is assumed to be defined by vertices in CCW order.
	*/
	intersection_result intersect_ray_convex_polygon(ray const& _ray, glm::vec3 const* _vertices, size_t const _size)
	{
		// Intersect ray against plane formed by vertices, and compute the resulting point.
		glm::vec3 const plane_normal = glm::cross(_vertices[2] - _vertices[1], _vertices[0] - _vertices[1]);
		if (glm::epsilonEqual(glm::dot(plane_normal, _ray.dir), 0.0f, 0.00001f))
			return { -1.0f, 0, plane_normal };
		float const t = -glm::dot(_ray.origin - _vertices[1], plane_normal) / glm::dot(_ray.dir, plane_normal);
		glm::vec3 const p = _ray.origin + t * _ray.dir;

		//  Check if computed point is "outside" the boundary planes formed by the edges.
		for (size_t face_index = 0; face_index < _size-1; face_index++)
		{
			glm::vec3 const edge_normal = glm::cross(_vertices[face_index + 1] - _vertices[face_index], plane_normal);
			if (glm::dot(p - _vertices[face_index], edge_normal) > 0)
				return { -1.0f, 0, plane_normal };
		}
		// Separate check for last edge (to avoid using modulus)
		{
			glm::vec3 const edge_normal = glm::cross(_vertices[0] - _vertices[_size - 1], plane_normal);
			if (glm::dot(p - _vertices[_size - 1], edge_normal) > 0)
				return { -1.0f, 0, plane_normal };
		}
		return { t, 0, plane_normal };
	}


	result_convex_hull_intersection intersect_convex_hulls_sat(convex_hull const& _hull1, transform3D _transform1, convex_hull const& _hull2, transform3D _transform2)
	{
		// Possible optimization: transform the hull with the least vertices to the space of the hull with the most vertices.
		return intersect_convex_hulls_sat(
			_hull1, _hull2,
			(_transform1.GetInverse() * _transform2).GetMatrix()
		);
	}

	result_convex_hull_intersection intersect_convex_hulls_sat(convex_hull const& _hull1, convex_hull const& _hull2, glm::mat4 const _mat_2_to_1)
	{
		std::vector<glm::vec3> const & hull1_vertices = _hull1.m_vertices;
		// Rather than performing the intersection in world space, we perform the intersection in the local space of one
		// of the passed convex hulls. This way, we only have to transform one set of vertices rather than both.
		std::vector<glm::vec3> hull2_vertices(_hull2.m_vertices);
		for (size_t i = 0; i < hull2_vertices.size(); i++)
			hull2_vertices[i] = _mat_2_to_1 * glm::vec4(hull2_vertices[i], 1.0f);

		result_convex_hull_intersection intersection_result;

		using edge_idx = convex_hull::half_edge_idx;
		using face_idx = convex_hull::face_idx;

		// Compute face normals for both hulls.
		std::vector<glm::vec3> hull1_normals(_hull1.m_faces.size()), hull2_normals(_hull2.m_faces.size());
		for (size_t i = 0; i < _hull1.m_faces.size(); i++)
			hull1_normals[i] = compute_convex_hull_face_normal(hull1_vertices, _hull1.m_faces, i);
		for (size_t i = 0; i < _hull2.m_faces.size(); i++)
			hull2_normals[i] = compute_convex_hull_face_normal(hull2_vertices, _hull2.m_faces, i);


		// Check for face-face intersections
		for (face_idx h1_f_idx = 0; h1_f_idx < _hull1.m_faces.size(); h1_f_idx++)
		{

		}
		for (face_idx h2_f_idx = 0; h2_f_idx < _hull1.m_faces.size(); h2_f_idx++)
		{

		}

		// Check for edge-edge intersections.
		for (edge_idx h1_e_idx = 0; h1_e_idx < _hull1.m_edges.size(); h1_e_idx++)
		{
			convex_hull::half_edge const h1_edge = _hull1.m_edges[h1_e_idx];
			convex_hull::half_edge const h1_twin_edge = _hull1.m_edges[h1_edge.m_twin_edge];
			glm::vec3 const A = hull1_normals[h1_edge.m_edge_face], B = hull1_normals[h1_twin_edge];

			glm::vec3 const cross_b_a = glm::cross(B, A);

			for (edge_idx h2_e_idx = 0; h2_e_idx < _hull2.m_edges.size(); h2_e_idx++)
			{
				convex_hull::half_edge const h2_edge = _hull2.m_edges[h2_e_idx];
				convex_hull::half_edge const h2_twin_edge = _hull2.m_edges[h2_edge.m_twin_edge];
				glm::vec3 const C = hull1_normals[h2_edge.m_edge_face], D = hull1_normals[h2_twin_edge];

				glm::vec3 const cross_d_c = glm::cross(D,C);
				glm::vec3 const cross_c_b = glm::cross(C, B);

				bool arcs_intersect = (
					glm::dot(glm::dot(C,cross_b_a), glm::dot(D,cross_b_a)) < 0.0f &&
					glm::dot(glm::dot(A, cross_d_c), glm::dot(B, cross_d_c)) < 0.0f &&
					glm::dot(glm::dot(A, cross_c_b), glm::dot(D, cross_c_b)) > 0.0f
				);

				if (arcs_intersect)
				{
					float sep = 0.0f;
					if (sep > 0.0f)
					{
						intersection_result.collision_type = ECollideType::eNoCollision;
						goto end;
					}
				}
			}
		}

		end:

		return intersection_result;
	}

}
}