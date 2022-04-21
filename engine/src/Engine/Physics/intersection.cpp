#include "intersection.h"

#include <Engine/Physics/contact.h>
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
	*			Ray does not collide with back faces.
	*/
	intersection_result intersect_ray_convex_hull(
		ray _ray, half_edge_data_structure const & _hds, 
		transform3D const& _hull_transform
	)
	{
		// Transform ray into local space of convex hull object.
		glm::mat4 const inv_mat = _hull_transform.GetInvMatrix();
		glm::mat2x4 const data = inv_mat * glm::mat2x4(glm::vec4(_ray.origin, 1.0f), glm::vec4(_ray.origin + _ray.dir, 1.0f));
		_ray.origin = data[0];
		_ray.dir = data[1] - data[0];

		for (half_edge_data_structure::face_idx face_index = 0; face_index < _hds.m_faces.size(); face_index++)
		{
			auto const& face = _hds.m_faces[face_index];
			glm::vec3 const face_normal = _hds.compute_face_normal(face_index);

			glm::vec3 const vtx_0 = _hds.m_vertices[face.m_vertices[0]];
			glm::vec3 vtx_1 = _hds.m_vertices[face.m_vertices[1]];

			// Skip face if ray is pointing away from face or starting point is behind plane.
			if(glm::dot(face_normal, _ray.dir) >= 0.0f || glm::dot(face_normal, _ray.origin - vtx_0) <= 0.0f)
				continue;

			// Intersect ray against plane formed by convex face first.
			float const face_t = glm::dot(vtx_0 - _ray.origin, face_normal) / glm::dot(_ray.dir, face_normal);

			glm::vec3 P = _ray.origin + face_t * _ray.dir;
			bool intersects = false;

			// Check whether ray intersects any triangle defined by convex face.
			for (size_t i = 2; i < face.m_vertices.size(); i++)
			{
				glm::vec3 const vtx_2 = _hds.m_vertices[face.m_vertices[i]];
				// Length of triangle normal is important for barycentric coordinate computation.
				// Thats why we compute the triangle normal even though we have the face normal.
				glm::vec3 const tri_normal = glm::cross(vtx_1 - vtx_0, vtx_2 - vtx_1);
				float const reciprocal_tri_normal_length = 1.0f / glm::length(tri_normal);

				// Inline triangle intersection.
				//glm::vec3 const P_01 = glm::cross(vtx_1 - vtx_0, P - vtx_1);
				glm::vec3 const P_12 = glm::cross(vtx_2 - vtx_1, P - vtx_2);
				glm::vec3 const P_20 = glm::cross(vtx_0 - vtx_2, P - vtx_0);

				float const P_12_length = glm::length(P_12);
				float const P_20_length = glm::length(P_20);

				float const u = P_12_length * reciprocal_tri_normal_length;
				float const v = P_20_length * reciprocal_tri_normal_length;

				intersects |= (u >= 0.0f && v >= 0.0f && u + v <= 1.0f);

				vtx_1 = vtx_2;
			}

			if (intersects)
				return intersection_result{
					.t = face_t,
					.face_index = face_index,
					.normal = glm::transpose(inv_mat) * glm::vec4(face_normal,0.0f),
				};
		}

		return intersection_result{
				.t = -1.0f,
				.face_index = (uint32_t)-1
		};
	}

	EIntersectionType intersect_convex_hulls_sat(
		half_edge_data_structure const& _hull1, transform3D const & _transform1, uint16_t _entity_id_1, 
		half_edge_data_structure const& _hull2, transform3D const & _transform2, uint16_t _entity_id_2,
		contact * _out_contacts, size_t * _out_contact_count, 
		bool* _reference_is_hull1
	)
	{

		using edge_idx = half_edge_data_structure::half_edge_idx;
		using face_idx = half_edge_data_structure::face_idx;
		using vertex_idx = half_edge_data_structure::vertex_idx;
		using face = half_edge_data_structure::face;
		using half_edge = half_edge_data_structure::half_edge;

		struct edge_pair {
			edge_idx hull1_edge_idx = std::numeric_limits<edge_idx>::max();
			edge_idx hull2_edge_idx = std::numeric_limits<edge_idx>::max();

			bool valid() const { return hull1_edge_idx != std::numeric_limits<edge_idx>::max() && hull2_edge_idx != hull1_edge_idx; }
		};
		struct face_vertex_pair {
			face_idx reference_face_index = std::numeric_limits<face_idx>::max();
			vertex_idx incident_face_vertex = std::numeric_limits<vertex_idx>::max();
			bool reference_is_hull1 = false;

			bool valid() const { return reference_face_index != std::numeric_limits<face_idx>::max() && incident_face_vertex != std::numeric_limits<vertex_idx>::max(); }
		};

		// Used later for edge-edge intersection tests.
		glm::vec3 hull1_center(0.0f);

		std::vector<glm::vec3> hull1_vertices(_hull1.m_vertices);
		std::vector<glm::vec3> hull2_vertices(_hull2.m_vertices);

		glm::mat4 const mat_1_to_world = _transform1.GetMatrix();
		glm::mat4 const mat_2_to_world = _transform2.GetMatrix();

		for (size_t i = 0; i < hull1_vertices.size(); i++)
		{
			hull1_vertices[i] = mat_1_to_world * glm::vec4(hull1_vertices[i], 1.0f);
			hull1_center += hull1_vertices[i];
		}
		for (size_t i = 0; i < hull2_vertices.size(); i++)
			hull2_vertices[i] = mat_2_to_world * glm::vec4(hull2_vertices[i], 1.0f);
		hull1_center /= float( hull1_vertices.size());


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

				glm::vec3 const cross_c_b = glm::cross(C, B);
				glm::vec3 const cross_d_b = glm::cross(D, B);
				glm::vec3 const cross_d_c = glm::cross(D, C);

				bool arcs_intersect = (
					(glm::dot(A, cross_c_b) * glm::dot(A, cross_d_b) < 0.0f) &&
					(glm::dot(A, cross_d_c) * glm::dot(B, cross_d_c) < 0.0f) &&
					(glm::dot(A, cross_c_b) * glm::dot(B, cross_d_c) < 0.0f)
				);

				// If true, separation is signed distance between intersection edges.
				// If it is negative, the penetration is the separation.
				if (!arcs_intersect)
					continue;

				glm::vec3 const e1_vec = hull1_vertices[h1_twin_edge.m_vertex] - hull1_vertices[h1_edge.m_vertex];
				glm::vec3 const e2_vec = hull2_vertices[h2_twin_edge.m_vertex] - hull2_vertices[h2_edge.m_vertex];

				// Compute separation plane between edges
				glm::vec3 separating_plane_normal = glm::normalize(glm::cross(e1_vec, e2_vec));

				if (glm::dot(separating_plane_normal, hull1_vertices[h1_twin_edge.m_vertex] - hull1_center) < 0.0f)
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

		EIntersectionType return_intersection_type = EIntersectionType::eNoIntersection;

		if (_out_contacts && min_sep_face_pair.valid())
		{
			auto const& reference_hull = min_sep_face_pair.reference_is_hull1 ? _hull1 : _hull2;
			auto const& incident_hull = !min_sep_face_pair.reference_is_hull1 ? _hull1 : _hull2;
			auto const& reference_vertices = min_sep_face_pair.reference_is_hull1 ? hull1_vertices : hull2_vertices;
			auto const& incident_vertices = !min_sep_face_pair.reference_is_hull1 ? hull1_vertices : hull2_vertices;
			face_idx const reference_face_index = min_sep_face_pair.reference_face_index;

			// Find incident face that is most anti-parallel to reference face.
			face_idx incident_face_index = -1;
			float min_dot = std::numeric_limits<float>::max();
			glm::vec3 const reference_face_normal = compute_hds_face_normal(reference_vertices, reference_hull.m_faces, reference_face_index);
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
					ref_sideplane_normals[i] = compute_hds_face_normal(reference_vertices, reference_hull.m_faces, ref_sideplane_edge.m_edge_face);
				}
			}

			uint16_t const reference_entity_id = _reference_is_hull1 ? _entity_id_1 : _entity_id_2;
			uint16_t const incident_entity_id = !_reference_is_hull1 ? _entity_id_1 : _entity_id_2;

			// Copy vertices of current incident face to vector.
			// Create list of contacts to clip.
			std::vector<contact> contacts_to_clip(incident_face.m_vertices.size());
			for (int i = 0; i < contacts_to_clip.size(); i++)
			{
				contact & new_contact = contacts_to_clip[i];
				contact_identifier identifier;
				identifier.edge_index_1 = incident_face.m_edges[(i - 1) % contacts_to_clip.size()];
				identifier.edge_index_2 = incident_face.m_edges[i];
				identifier.entity_id_1 = incident_entity_id;
				identifier.entity_id_2 = incident_entity_id;
				new_contact.point = incident_vertices[incident_face.m_vertices[i]];
				new_contact.normal = reference_face_normal;
				new_contact.identifier = identifier;
				// Set penetration later
			}

			// Clip segments formed by point list against sideplanes one-by-one.
			// Sutherland-Hodgeman clipping algorithm.
			std::vector<contact> clipped_contacts(std::move(contacts_to_clip));
			{
				for (int clipping_sideplane_idx = 0; clipping_sideplane_idx < ref_sideplane_vertices.size(); clipping_sideplane_idx++)
				{
					contacts_to_clip = std::move(clipped_contacts);
					glm::vec3 const sideplane_n = ref_sideplane_normals[clipping_sideplane_idx];
					glm::vec3 const sideplane_p = ref_sideplane_vertices[clipping_sideplane_idx];

					for (int i = 0; i < contacts_to_clip.size(); i++)
					{
						int const prev_point_idx = (i == 0) ? contacts_to_clip.size() - 1 : i-1;
						glm::vec3 const contact_points[2] = { 
							contacts_to_clip[prev_point_idx].point,
							contacts_to_clip[i].point
						};
						glm::vec3 const & cp_prev = contact_points[0];
						glm::vec3 const & cp_current = contact_points[1];
						contact_identifier const identifier_curr = contacts_to_clip[1].identifier;

						float const intersection_t = -glm::dot(cp_prev - sideplane_p, sideplane_n) / glm::dot(cp_current - cp_prev, sideplane_n);

						bool contact_points_inside[2];
						points_behind_planes(
							&ref_sideplane_vertices[clipping_sideplane_idx],
							&ref_sideplane_normals[clipping_sideplane_idx],
							1,
							contact_points,
							2,
							contact_points_inside
						);
						if (contact_points_inside[1])
						{
							if (!contact_points_inside[0])
							{
								contact contact_intersect;
								contact_intersect.point = cp_prev + intersection_t * (cp_current - cp_prev);
								contact_intersect.normal = reference_face_normal;
								contact_intersect.identifier = identifier_curr;
								contact_intersect.identifier.entity_id_1 = reference_entity_id;
								contact_intersect.identifier.edge_index_1 = reference_face.m_edges[clipping_sideplane_idx];
								clipped_contacts.emplace_back(contact_intersect);
							}
							clipped_contacts.emplace_back(contacts_to_clip[i]);
						}
						else if (contact_points_inside[0])
						{
							contact contact_intersect;
							contact_intersect.point = cp_prev + intersection_t * (cp_current - cp_prev);
							contact_intersect.normal = reference_face_normal;
							contact_intersect.identifier = identifier_curr;
							contact_intersect.identifier.entity_id_2 = reference_entity_id;
							contact_intersect.identifier.edge_index_2 = reference_face.m_edges[clipping_sideplane_idx];
							clipped_contacts.emplace_back(contact_intersect);
						}
					}

				}
			}

			// Compute penetration of each clipped point.
			// Only keep contact points with positive penetration
			std::vector<contact> contact_points;
			contact_points.reserve(clipped_contacts.size());

			for (size_t i = 0; i < clipped_contacts.size(); i++)
			{
				contact & icp = clipped_contacts[i];
				icp.penetration = -glm::dot(icp.point - reference_face_vtx, icp.normal);

				if (icp.penetration > glm::epsilon<float>())
					contact_points.emplace_back(icp);
			}

			//assert(contact_vec.empty() && "All points have been clipped - bug in implementation.");

			// Output data
			*_out_contact_count = contact_points.size();
			memcpy(_out_contacts, contact_points.data(), contact_points.size() * sizeof(contact));
			*_reference_is_hull1 = min_sep_face_pair.reference_is_hull1;

			return_intersection_type = EIntersectionType::eFaceIntersection;
		}
		else if (_out_contacts && min_sep_edge_pair.valid())
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

			// Compute contact points in world space.			
			glm::vec3 intersection_points[2] = { p1 + t1 * v1, p2 + t2 * v2};

			float distance = glm::distance(intersection_points[0], intersection_points[1]);

			contact new_contact;
			new_contact.point = intersection_points[0];
			new_contact.normal = (intersection_points[1] - intersection_points[0]) / distance;
			new_contact.penetration = distance;
			contact_identifier new_identifier;
			new_identifier.entity_id_1 = _entity_id_1;
			new_identifier.entity_id_2 = _entity_id_2;
			new_identifier.edge_index_1 = h1_e_idx;
			new_identifier.edge_index_2 = h2_e_idx;

			// Output data.
			_out_contacts[0] = new_contact;
			*_out_contact_count = 1;
			*_reference_is_hull1 = false;

			return_intersection_type = EIntersectionType::eEdgeIntersection;
		}


		return return_intersection_type;
	}


	void points_behind_planes(
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
				inside_planes &= dot < glm::epsilon<float>();
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