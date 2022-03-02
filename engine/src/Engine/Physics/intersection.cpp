#include "intersection.h"

#include <glm/gtx/norm.hpp>

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
	* @param	size_t			Size of support_vertex array
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
		// Separate check for last incident_edge (to avoid using modulus)
		{
			glm::vec3 const edge_normal = glm::cross(_vertices[0] - _vertices[_size - 1], plane_normal);
			if (glm::dot(p - _vertices[_size - 1], edge_normal) > 0)
				return { -1.0f, 0, plane_normal };
		}
		return { t, 0, plane_normal };
	}


	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, transform3D _transform1, 
		half_edge_data_structure const& _hull2, transform3D _transform2,
		contact_manifold* _out_contact_manifold
	)
	{
		// Possible optimization: transform the hull with the least vertices to the space of the hull with the most vertices.
		return intersect_convex_hulls_sat(
			_hull1, _hull2,
			(_transform1.GetInverse() * _transform2),
			_out_contact_manifold
		);
	}

	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, 
		half_edge_data_structure const& _hull2, 
		transform3D const& _transform_2_to_1,
		contact_manifold* _out_contact_manifold
	)
	{

		glm::mat4 const mat_2_to_1 = _transform_2_to_1.GetMatrix();
		glm::mat4 const mat_1_to_2 = _transform_2_to_1.GetInvMatrix();

		using edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::face_idx;
		using vertex_idx = half_edge_data_structure::vertex_idx;
		using face = half_edge_data_structure::face;
		using half_edge = half_edge_data_structure::half_edge;

		struct edge_pair { edge_idx hull1_edge_idx, hull2_edge_idx; };
		struct face_vertex_pair { face_idx reference_face_index; vertex_idx incident_face_vertex; bool reference_is_hull1 = false; };

		std::vector<glm::vec3> const & hull1_vertices = _hull1.m_vertices;
		// Rather than performing the intersection in world space, we perform the intersection in the local space of one
		// of the passed convex hulls. This way, we only have to transform one set of vertices rather than both.
		std::vector<glm::vec3> hull2_vertices(_hull2.m_vertices);
		for (size_t i = 0; i < hull2_vertices.size(); i++)
			hull2_vertices[i] = mat_2_to_1 * glm::vec4(hull2_vertices[i], 1.0f);

		float minimum_separation = -std::numeric_limits<float>::max();
		face_vertex_pair min_sep_face_pair;
		edge_pair min_sep_edge_pair{ half_edge::INVALID_EDGE, half_edge::INVALID_EDGE };

		// Compute face normals for both hulls.
		// TODO: Precompute face normals?
		std::vector<glm::vec3> hull1_normals(_hull1.m_faces.size()), hull2_normals(_hull2.m_faces.size());
		for (size_t i = 0; i < _hull1.m_faces.size(); i++)
			hull1_normals[i] = compute_hds_face_normal(hull1_vertices, _hull1.m_faces, i);
		for (size_t i = 0; i < _hull2.m_faces.size(); i++)
			hull2_normals[i] = compute_hds_face_normal(hull2_vertices, _hull2.m_faces, i);

		// Check for face-face separation
		for (face_idx h1_f_idx = 0; h1_f_idx < _hull1.m_faces.size(); h1_f_idx++)
		{
			glm::vec3 const face_normal = hull1_normals[h1_f_idx];
			vertex_idx const sup_vtx_idx = get_hds_support_point_bruteforce(
				hull2_vertices, -face_normal
			);
			glm::vec3 const support_vertex = hull2_vertices[sup_vtx_idx];
			glm::vec3 const proj_vertex = project_point_to_plane(
				hull1_vertices[_hull1.m_faces[h1_f_idx].m_vertices.front()], 
				-face_normal, 
				support_vertex
			);
			float const separation = glm::dot(-face_normal, proj_vertex - support_vertex);
			if (separation > 0.0f)
				return EIntersectionType::eNoIntersection;
			if (separation > minimum_separation)
			{
				minimum_separation = separation;
				min_sep_face_pair = face_vertex_pair{ h1_f_idx, sup_vtx_idx, true };
			}
		}
		for (face_idx h2_f_idx = 0; h2_f_idx < _hull2.m_faces.size(); h2_f_idx++)
		{
			glm::vec3 const face_normal = hull2_normals[h2_f_idx];
			vertex_idx const sup_vtx_idx = get_hds_support_point_bruteforce(
				hull1_vertices, -face_normal
			);
			glm::vec3 const support_vertex = hull1_vertices[sup_vtx_idx];
			glm::vec3 const proj_vertex = project_point_to_plane(
				hull2_vertices[_hull2.m_faces[h2_f_idx].m_vertices.front()],
				-face_normal,
				support_vertex
			);
			float const separation = glm::dot(-face_normal, proj_vertex - support_vertex);
			if (separation > 0.0f)
				return EIntersectionType::eNoIntersection;
			if (separation > minimum_separation)
			{
				minimum_separation = separation;
				min_sep_face_pair = face_vertex_pair{ h2_f_idx, sup_vtx_idx, false };
			}
		}

		// Check for incident_edge-incident_edge intersections.
		for (edge_idx h1_e_idx = 0; h1_e_idx < _hull1.m_edges.size(); h1_e_idx++)
		{
			half_edge const h1_edge = _hull1.m_edges[h1_e_idx];
			half_edge const h1_twin_edge = _hull1.m_edges[h1_edge.m_twin_edge];
			glm::vec3 const A = hull1_normals[h1_edge.m_edge_face];
			glm::vec3 const B = hull1_normals[h1_twin_edge.m_edge_face];

			glm::vec3 const cross_b_a = glm::cross(B, A);

			for (edge_idx h2_e_idx = 0; h2_e_idx < _hull2.m_edges.size(); h2_e_idx++)
			{
				half_edge const h2_edge = _hull2.m_edges[h2_e_idx];
				half_edge const h2_twin_edge = _hull2.m_edges[h2_edge.m_twin_edge];
				glm::vec3 const C = hull2_normals[h2_twin_edge.m_edge_face];
				glm::vec3 const D = hull2_normals[h2_edge.m_edge_face];

				glm::vec3 const cross_d_c = glm::cross(D, C);
				glm::vec3 const cross_c_b = glm::cross(C, B);
				
				float const c_dot_cross_b_a = glm::dot(C, cross_b_a);
				float const b_dot_cross_d_c = glm::dot(B, cross_d_c);
				
				bool arcs_intersect = (
					(c_dot_cross_b_a * glm::dot(D, cross_b_a) < 0.0f) &&
					(glm::dot(A, cross_d_c) *  b_dot_cross_d_c < 0.0f) &&
					(c_dot_cross_b_a * b_dot_cross_d_c) < 0.0f
				);

				// If true, separation is signed distance between intersection edges.
				// If it is negative, the penetration is the separation.
				if (!arcs_intersect)
					continue;

				glm::vec3 const e1_vec = hull1_vertices[h1_twin_edge.m_vertex] - hull1_vertices[h1_edge.m_vertex];
				glm::vec3 const e2_vec = hull2_vertices[h2_twin_edge.m_vertex] - hull2_vertices[h2_edge.m_vertex];

				// Compute separation plane between edges
				glm::vec3 separating_plane_normal = glm::normalize(glm::cross(e1_vec, e2_vec));

				// Compute center of current face. TODO: Cache?
				glm::vec3 hull1_face_center(0.0f);
				/*auto const & h1_e_face_vertices = _hull1.m_faces[h1_edge.m_edge_face].m_vertices;
				for (vertex_idx vtx_idx : h1_e_face_vertices)
					hull1_face_center += hull1_vertices[vtx_idx];
				hull1_face_center /= float(h1_e_face_vertices.size());*/

				if (glm::dot(separating_plane_normal, hull1_vertices[h1_twin_edge.m_vertex] - hull1_face_center) < 0.0f)
					separating_plane_normal = -separating_plane_normal;

				// Signed distance to separating plane from hull2 incident_edge vertex is separation.
				float const separation = glm::dot(separating_plane_normal, hull2_vertices[h2_edge.m_vertex] - hull1_vertices[h1_edge.m_vertex]);
				if (separation > 0.0f)
					return EIntersectionType::eNoIntersection;
				if (separation > minimum_separation)
				{
					minimum_separation = separation;
					min_sep_edge_pair = edge_pair{ h1_e_idx, h2_e_idx };
				}
			}
		}

		EIntersectionType return_intersection_type;
		return_intersection_type = (min_sep_edge_pair.hull1_edge_idx != half_edge::INVALID_EDGE)
			? EIntersectionType::eEdgeIntersection 
			: EIntersectionType::eFaceIntersection;
		
		if (_out_contact_manifold && return_intersection_type == EIntersectionType::eEdgeIntersection)
		{
			auto const [h1_e_idx, h2_e_idx] = min_sep_edge_pair;
			half_edge const h1_edge = _hull1.m_edges[h1_e_idx];
			half_edge const h2_edge = _hull2.m_edges[h2_e_idx];
			glm::vec3 const p1 = hull1_vertices[h1_edge.m_vertex];
			glm::vec3 const v1 = hull1_vertices[_hull1.m_edges[h1_edge.m_next_edge].m_vertex] - p1;
			glm::vec3 const p2 = hull2_vertices[h2_edge.m_vertex];
			glm::vec3 const v2 = hull2_vertices[_hull2.m_edges[h2_edge.m_next_edge].m_vertex] - p2;

			glm::vec3 const diff = p1 - p2;
			float const a = glm::length2(v1);
			float const b = glm::dot(v1, v2);
			float const c = glm::length2(v2);
			float const d = glm::dot(v1, diff);
			float const e = glm::dot(v2, diff);
			
			// Clamp to line segments defined by hull edges
			// Clamp between [0,1] since t=0 is current incident_edge vertex and t=1 is next incident_edge vertex.

			// Assume parallel incident_edge case does not occur since gauss map should not trigger
			// for parallel edges.

			float t2 = (b * d - a * e) / (b * b - a * c);
			t2 = std::clamp(t2, 0.0f, 1.0f);
			float t1 = (-d + b * t2) / a;
			t1 = std::clamp(t1, 0.0f, 1.0f);
			t2 = (e + t1 * b) / c;
			t2 = std::clamp(t2, 0.0f, 1.0f);

			
			_out_contact_manifold->is_edge_edge = true;
			_out_contact_manifold->edge_edge_contact.hull1_edge_idx = h1_e_idx;
			_out_contact_manifold->edge_edge_contact.hull2_edge_idx = h2_e_idx;
			_out_contact_manifold->edge_edge_contact.hull1_edge_t = t1;
			_out_contact_manifold->edge_edge_contact.hull2_edge_t = t2;
		}
		else if(_out_contact_manifold && return_intersection_type == EIntersectionType::eFaceIntersection)
		{
			auto const& reference_hull = min_sep_face_pair.reference_is_hull1 ? _hull1 : _hull2;
			auto const & incident_hull = !min_sep_face_pair.reference_is_hull1 ? _hull1 : _hull2;
			auto const& reference_vertices = min_sep_face_pair.reference_is_hull1 ? hull1_vertices : hull2_vertices;
			auto const & incident_vertices = !min_sep_face_pair.reference_is_hull1 ? hull1_vertices : hull2_vertices;
			face_idx const reference_face_index = min_sep_face_pair.reference_face_index;

			// Find incident face that is most anti-parallel to reference face.
			face_idx incident_face_index = -1;
			float min_dot = std::numeric_limits<float>::max();
			glm::vec3 const reference_face_normal = glm::normalize(compute_hds_face_normal(reference_vertices, reference_hull.m_faces, reference_face_index));
			for (face_idx incident_hull_face_idx = 0; incident_hull_face_idx < incident_hull.m_faces.size(); ++incident_hull_face_idx)
			{
				float const dot = glm::dot(compute_hds_face_normal(incident_vertices, incident_hull.m_faces, incident_hull_face_idx), reference_face_normal);
				if (dot < min_dot)
				{
					min_dot = dot;
					incident_face_index = incident_hull_face_idx;
				}
			}

			face const& reference_face = reference_hull.m_faces[reference_face_index];
			face const& incident_face = incident_hull.m_faces[incident_face_index];

			glm::vec3 const reference_face_vtx = reference_vertices[reference_face.m_vertices.front()];

			// Collect sideplane normals and sideplane vertices for ease of computation
			std::vector<glm::vec3> ref_sideplane_normals(reference_face.m_edges.size());
			std::vector<glm::vec3> ref_sideplane_vertices(reference_face.m_vertices.size());
			{
				size_t const ref_face_vtx_count = reference_face.m_vertices.size();
				for (size_t i = 0; i < ref_face_vtx_count; i++)
				{
					half_edge const ref_face_edge = reference_hull.m_edges[reference_face.m_edges[i]];
					half_edge const ref_sideplane_edge = reference_hull.m_edges[ref_face_edge.m_twin_edge];
					ref_sideplane_vertices[i] = reference_vertices[reference_face.m_vertices[i]];
					ref_sideplane_normals[i] = glm::normalize(
						compute_hds_face_normal(reference_vertices, reference_hull.m_faces, ref_sideplane_edge.m_edge_face)
					);
				}
			}

			// Copy vertices of current incident face to vector.
			std::vector<glm::vec3> vertices_to_clip(incident_face.m_vertices.size());
			for (size_t i = 0; i < incident_face.m_vertices.size(); i++)
				vertices_to_clip[i] = incident_vertices[incident_face.m_vertices[i]];

			// Clip segments formed by point list against sideplanes one-by-one.
			std::vector<glm::vec3> clipped_vertices;
			{
				// Allocate block for storing clipped vertices on stack.
				size_t const MAX_TEMP_VERTICES = 64;
				bool vtx_inside[MAX_TEMP_VERTICES];
				glm::vec3 tmp_clipped_vertices[MAX_TEMP_VERTICES];
				size_t iter_clipped_vertices = 0;

				for (size_t i = 0; i < ref_sideplane_vertices.size(); i++)
				{
					glm::vec3 const sideplane_vtx = ref_sideplane_vertices[i];
					glm::vec3 const sideplane_normal = ref_sideplane_normals[i];

					iter_clipped_vertices = 0;

					points_inside_planes(
						&ref_sideplane_vertices[i], &ref_sideplane_normals[i], 1,
						vertices_to_clip.data(), vertices_to_clip.size(), vtx_inside
					);

					// Sutherland-Hodgeman algorithm
					for (size_t j = 0; j < vertices_to_clip.size(); j++)
					{
						size_t const k = (j + 1) % (vertices_to_clip.size());
						glm::vec3 const p1 = vertices_to_clip[j]; // Prev
						glm::vec3 const p2 = vertices_to_clip[k]; // Current

						if (vtx_inside[j] != vtx_inside[k])
						{
							float const t = intersect_segment_planes(
								p1, p2, &sideplane_vtx, &sideplane_normal, 1
							);
							tmp_clipped_vertices[iter_clipped_vertices++] = p1 + t * (p2 - p1);
						}

						if (!vtx_inside[k])
							tmp_clipped_vertices[iter_clipped_vertices++] = p2;
					}

					// Load clipped vertices back into vertices_to_clip.
					vertices_to_clip.resize(iter_clipped_vertices);
					for (size_t i = 0; i < iter_clipped_vertices; i++)
						vertices_to_clip[i] = tmp_clipped_vertices[i];
				}

				// Load clipped vertices into final vector for processing.
				clipped_vertices.resize(iter_clipped_vertices);
				for (size_t i = 0; i < iter_clipped_vertices; i++)
					clipped_vertices[i] = tmp_clipped_vertices[i];
			}
			
			// Compute penetration of each clipped vertex.
			std::vector<float> vertex_penetrations(clipped_vertices.size());
			for (size_t i = 0; i < clipped_vertices.size(); i++)
				vertex_penetrations[i] = -glm::dot(clipped_vertices[i] - reference_face_vtx, reference_face_normal);

			// Only keep vertices with positive penetration
			size_t discarded_vertices = 0;
			for (size_t i = 0; i < clipped_vertices.size() - discarded_vertices; i++)
			{
				size_t const back_index = clipped_vertices.size() - discarded_vertices - 1;
				if (vertex_penetrations[i] < -glm::epsilon<float>())
				{
					std::swap(vertex_penetrations[i], vertex_penetrations[back_index]);
					std::swap(clipped_vertices[i], clipped_vertices[back_index]);
					discarded_vertices++;
				}
			}

			assert(discarded_vertices < clipped_vertices.size() && "All vertices have been clipped - bug in implementation.");

			vertex_penetrations.erase(vertex_penetrations.end() - discarded_vertices, vertex_penetrations.end());
			clipped_vertices.erase(clipped_vertices.end() - discarded_vertices, clipped_vertices.end());


			// If hull2 is the reference hull, transform points back to hull2 local space
			if (!min_sep_face_pair.reference_is_hull1)
				for (glm::vec3& p : clipped_vertices)
					p = mat_1_to_2 * glm::vec4(p,1.0f);

			// Output data
			_out_contact_manifold->face_face_contact.reference_face_idx = reference_face_index;
			_out_contact_manifold->face_face_contact.incident_face_idx = incident_face_index;
			_out_contact_manifold->vertex_penetrations = std::move(vertex_penetrations);
			_out_contact_manifold->incident_vertices = std::move(clipped_vertices);
			_out_contact_manifold->reference_is_hull_1 = min_sep_face_pair.reference_is_hull1;
		}


		return return_intersection_type;
	}

	void points_inside_planes(
		glm::vec3 const* _plane_vertices, 
		glm::vec3 const* _plane_normals, 
		size_t const _plane_count,
		glm::vec3 const * _p, 
		size_t const _point_count,
		bool* _out_results
	)
	{
		for (size_t i = 0; i < _point_count; i++)
		{
			bool inside_planes = true;
			for (size_t j = 0; j < _plane_count; j++)
			{
				float const dot = glm::dot(_p[i] - _plane_vertices[j], _plane_normals[j]);
				inside_planes &= dot > -glm::epsilon<float>();
			}
			_out_results[i] = inside_planes;
		}
	}

	float intersect_segment_planes(
		glm::vec3 const _p1, glm::vec3 const _p2, 
		glm::vec3 const* _plane_vertices, 
		glm::vec3 const* _plane_normals,
		size_t const _plane_count
	)
	{
		float min_t = 1.0f;
		for (size_t i = 0; i < _plane_count; i++)
		{
			float intersect_t = -glm::dot(_p1 - _plane_vertices[i], _plane_normals[i]) / glm::dot(_p2 - _p1, _plane_normals[i]);
			// Ignore intersections behind start of segment.
			if (intersect_t < 0.0f)
				intersect_t = std::numeric_limits<float>::max();
			min_t = std::min(min_t, std::clamp(intersect_t, 0.0f, 1.0f));
		}
		return min_t;
	}

}
}