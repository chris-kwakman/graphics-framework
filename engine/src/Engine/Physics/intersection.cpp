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

}
}