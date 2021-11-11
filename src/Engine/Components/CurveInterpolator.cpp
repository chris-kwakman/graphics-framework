#include "CurveInterpolator.h"

#include <ImGuizmo/ImGuizmo.h>
#include <Engine/Editor/editor.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Transform.h>

#include <glm/gtx/matrix_decompose.hpp>

namespace Component
{
	const char* CurveInterpolatorManager::GetComponentTypeName() const
	{
		return "CurveInterpolator";
	}

	decltype(CurveInterpolatorManager::m_renderable_curves) const& CurveInterpolatorManager::GetRenderableCurves() const
	{
		return m_renderable_curves;
	}

	decltype(CurveInterpolatorManager::m_renderable_curve_nodes) const& CurveInterpolatorManager::GetRenderableCurveNodes() const
	{
		return m_renderable_curve_nodes;
	}

	void CurveInterpolatorManager::impl_clear()
	{
		m_map.clear();
		m_renderable_curves.clear();
		m_renderable_curve_nodes.clear();
	}

	bool CurveInterpolatorManager::impl_create(Entity _e)
	{
		piecewise_curve new_curve;
		new_curve.m_nodes.clear();
		new_curve.set_curve_type(piecewise_curve::EType::Linear);

		m_map.emplace(_e, std::move(new_curve));
		m_renderable_curves.insert(_e);
		m_renderable_curve_nodes.insert(_e);

		return true;
	}

	void CurveInterpolatorManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			m_map.erase(_entities[i]);
			m_renderable_curves.erase(_entities[i]);
			m_renderable_curve_nodes.erase(_entities[i]);
		}
	}

	bool CurveInterpolatorManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_map.find(_entity) != m_map.end();
	}

	using curve_type = piecewise_curve::EType;

	std::unordered_map<curve_type, const char*> s_curve_type_names = {
		{curve_type::Linear, "Linear"},
		{curve_type::Hermite, "Hermite"},
		{curve_type::Catmull, "Catmull"},
		{curve_type::Bezier, "Bezier"}
	};

	static const char* get_curve_type_name(curve_type _type)
	{
		return s_curve_type_names.at(_type);
	}

	void CurveInterpolatorManager::impl_edit_component(Entity _entity)
	{
		if (ImGui::IsItemFocused())
			Singleton<Engine::Editor::Editor>().ComponentUsingImguizmoWidget = GetComponentTypeName();

		static int curve_resolution = 20;
		static int const MAX_POINTS_DISPLAYED = 7;
		static bool show_curve_point_list = true;
		static bool show_distance_lut_table = false;
		static Entity editing_entity = Entity::InvalidEntity;

		piecewise_curve& curve = m_map.at(_entity);

		bool rendering_curve = (m_renderable_curves.find(_entity) != m_renderable_curves.end());
		bool rendering_curve_nodes = (m_renderable_curve_nodes.find(_entity) != m_renderable_curve_nodes.end());

		auto mirror_curve_node = [&](int node_index)
		{
			// If we are editing curve w/ tangent / intermediate nodes (Hermite / Bezier),
			// adjust node values to maintain continuity.
			if (curve.m_type == curve_type::Hermite || curve.m_type == curve_type::Bezier)
			{
				int given_remainder = node_index % 3;
				if (given_remainder == 1 && node_index > 1)
				{
					float length = 0.0f;
					if (glm::all(glm::epsilonEqual(curve.m_nodes[node_index - 2], glm::vec3(0.0f), glm::epsilon<float>())))
						length = 0.0f;
					else
						length = glm::length(curve.m_nodes[node_index - 2]);
					curve.m_nodes[node_index - 2] = -length * glm::normalize(curve.m_nodes[node_index]);
				}
				else if (given_remainder == 2 && node_index < curve.m_nodes.size() - 2)
				{
					float length = 0.0f;
					if (glm::all(glm::epsilonEqual(curve.m_nodes[node_index + 2], glm::vec3(0.0f), glm::epsilon<float>())))
						length = 0.0f;
					else
						length = glm::length(curve.m_nodes[node_index + 2]);
					curve.m_nodes[node_index + 2] = -length * glm::normalize(curve.m_nodes[node_index]);
				}
			}
		};

		if (ImGui::Checkbox("Render Curve", &rendering_curve))
		{
			if (rendering_curve)
				m_renderable_curves.emplace(_entity);
			else
				m_renderable_curves.erase(_entity);
		}
		if (ImGui::Checkbox("Render Curve Nodes", &rendering_curve_nodes))
		{
			if (rendering_curve_nodes)
				m_renderable_curve_nodes.emplace(_entity);
			else
				m_renderable_curve_nodes.erase(_entity);
		}

		ImGui::Separator();

		if (ImGui::BeginCombo("Curve Type", get_curve_type_name(curve.m_type)))
		{
			for (unsigned int i = 0; i < curve_type::COUNT; ++i)
			{
				bool selected = false;
				curve_type curr_type = curve_type(i);
				if (curve.m_type == curr_type)
					selected = true;
				if (ImGui::Selectable(get_curve_type_name(curr_type), &selected))
				{
					curve.set_curve_type(curr_type);
					generate_curve_lut(&curve, &curve.m_lut, curve_resolution);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SliderInt("Curve Resolution", &curve_resolution, 2, 200);
		if (ImGui::Button("Regenerate LUT"))
			generate_curve_lut(&curve, &curve.m_lut, curve_resolution);
		ImGui::Checkbox("Show Curve Point List", &show_curve_point_list);
		ImGui::Checkbox("Show Distance LUT", &show_distance_lut_table);
		if (ImGui::Button("Add Curve Point"))
			curve.m_nodes.push_back(glm::vec3(0.0f));
		if (show_curve_point_list && ImGui::BeginListBox("Curve Nodes", ImVec2(0.0f, 0.0f)))
		{

			int delete_point_index = -1;
			int move_point_index = -1;
			bool move_up = true; // False if we should move point index down.

			static int transform_node_index = 0;
			transform_node_index = std::clamp(transform_node_index, -1, (int)curve.m_nodes.size() - 1);

			if (
				(transform_node_index >= 0 && transform_node_index < curve.m_nodes.size()) &&
				Singleton<Engine::Editor::Editor>().ComponentUsingImguizmoWidget == GetComponentTypeName()
				)
			{
				int const transform_idx_remainder = transform_node_index % 3;

				Engine::ECS::Entity const editor_cam_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
				auto editor_camera = editor_cam_entity.GetComponent<Component::Camera>();
				auto editor_camera_transform = editor_cam_entity.GetComponent<Component::Transform>();

				glm::mat4x4 const view_matrix = editor_camera_transform.ComputeWorldTransform().GetInvMatrix();
				glm::mat4x4 const perspective_matrix = editor_camera.GetCameraData().get_perspective_matrix();

				glm::vec3 node_position = curve.m_nodes[transform_node_index];
				if (curve.m_type == curve_type::Bezier || curve.m_type == curve_type::Hermite)
				{
					if (transform_idx_remainder == 1)
						node_position += curve.m_nodes[transform_node_index - 1];
					else if (transform_idx_remainder == 2)
						node_position += curve.m_nodes[transform_node_index + 1];
				}

				glm::mat4x4 transform_matrix = glm::translate(glm::identity<glm::mat4x4>(), node_position);

				if (ImGuizmo::Manipulate(
					(float*)&view_matrix[0][0], (float*)&perspective_matrix[0][0],
					ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
					(float*)&transform_matrix[0][0]
				))
				{
					glm::vec3 scale, translation, skew;
					glm::quat rotation;
					glm::vec4 perspective;
					glm::decompose(transform_matrix, scale, rotation, translation, skew, perspective);

					if (curve.m_type == curve_type::Bezier || curve.m_type == curve_type::Hermite)
					{
						if (transform_idx_remainder == 1)
							translation -= curve.m_nodes[transform_node_index - 1];
						else if (transform_idx_remainder == 2)
							translation -= curve.m_nodes[transform_node_index + 1];
					}

					curve.m_nodes[transform_node_index] = translation;

					generate_curve_lut(&curve, &curve.m_lut, curve_resolution);
					mirror_curve_node(transform_node_index);
				}

			}

			for (unsigned int i = 0; i < curve.m_nodes.size(); ++i)
			{
				int const remainder = i % 3;

				ImGui::PushID(i);
				char buffer[16];
				snprintf(buffer, sizeof(buffer), "###Point %i", i);

				if (ImGui::RadioButton("###TransformCurveNode", (transform_node_index == i)))
				{
					Singleton<Engine::Editor::Editor>().ComponentUsingImguizmoWidget = GetComponentTypeName();
					transform_node_index = i;
				}
				ImGui::SameLine();

				// Apply different color to distringuish between point-only curves and point+tangent curves
				float const style_color_mult = 0.5f;
				ImGuiCol const edit_imgui_elem_col = ImGuiCol_FrameBg;
				ImVec4 const default_style_col = ImGui::GetStyleColorVec4(edit_imgui_elem_col);
				ImVec4 const style_color_node_position = ImVec4(
					style_color_mult * COLOR_NODE_POSITION.x, 
					style_color_mult * COLOR_NODE_POSITION.y,
					style_color_mult * COLOR_NODE_POSITION.z,
					default_style_col.w
				);
				ImVec4 const style_color_node_tangent = ImVec4(
					style_color_mult * COLOR_NODE_TANGENT.x,
					style_color_mult * COLOR_NODE_TANGENT.y,
					style_color_mult * COLOR_NODE_TANGENT.z,
					default_style_col.w
				);
				if (curve.m_type == piecewise_curve::EType::Linear || curve.m_type == piecewise_curve::EType::Catmull)
				{
					ImGui::PushStyleColor(edit_imgui_elem_col, style_color_node_position);
				}
				else
				{
					ImGui::PushStyleColor(
						edit_imgui_elem_col, 
						(i % 3 == 0) ? style_color_node_position : style_color_node_tangent
					);
				}
				if (ImGui::DragFloat3(buffer, &curve.m_nodes[i].x, 0.01f, -FLT_MAX, FLT_MAX, "%.2f"))
				{
					generate_curve_lut(&curve, &curve.m_lut, curve_resolution);
					mirror_curve_node(i);
				}
				ImGui::PopStyleColor(1);
				ImGui::SameLine();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				if (ImGui::SmallButton("X"))
					delete_point_index = i;
				ImGui::PopStyleColor();
				ImGui::SameLine();

				if (i != curve.m_nodes.size() - 1)
				{
					ImGui::SameLine();
					ImGui::PushItemWidth(30.0f);
					if (ImGui::ArrowButton("Down", ImGuiDir_Down))
					{
						move_point_index = i;
						move_up = false;
					}
					ImGui::PopItemWidth();
				}
				else
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 30.0f);
				if (i != 0)
				{
					ImGui::SameLine();
					ImGui::PushItemWidth(30.0f);
					if (ImGui::ArrowButton("Up", ImGuiDir_Up))
					{
						move_point_index = i;
						move_up = true;
					}
					ImGui::PopItemWidth();
				}
				ImGui::PopID();
			}
			if (delete_point_index != -1)
				curve.m_nodes.erase(curve.m_nodes.begin() + delete_point_index);
			if (move_point_index != -1)
			{
				std::swap(
					curve.m_nodes.at(move_point_index), 
					curve.m_nodes.at(move_point_index + (int)!move_up - (int)move_up)
				);
			}
			ImGui::EndListBox();
		}
		if (show_distance_lut_table)
		{
			if (ImGui::BeginTable("Distance LUT", 2))
			{
				for (unsigned int i = 0; i < curve.m_lut.m_distances.size(); ++i)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", curve.m_lut.m_distances[i]);
					ImGui::TableNextColumn();
					glm::vec3 const point = curve.m_lut.m_points[i];
					ImGui::Text("%.2f, %.2f, %.2f", point.x, point.y, point.z);
				}
				ImGui::EndTable();
			}
		}
	}

	void CurveInterpolatorManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	void CurveInterpolatorManager::generate_curve_lut(piecewise_curve const* _curve, lookup_table* _lut, unsigned int _lut_resolution)
	{
		assert(_curve);
		assert(_lut);

		if (_curve->m_type == curve_type::Linear)
		{
			_lut->m_distances.resize(_curve->m_nodes.size());
			_lut->m_points.resize(_curve->m_nodes.size());
		}
		else
		{
			_lut->m_distances.resize(_lut_resolution);
			_lut->m_points.resize(_lut_resolution);
		}

		std::vector<glm::vec3> const& nodes = _curve->m_nodes;

		float const normalized_distance_delta = 1.0f / float(_lut_resolution-1);
		float normalized_distance = 0.0f;
		if (_curve->m_type == piecewise_curve::EType::Linear)
		{
			_lut->m_points = nodes;
		}
		else if (_curve->m_type == piecewise_curve::EType::Catmull && nodes.size() >= 2)
		{
			unsigned int const segment_count = (nodes.size() - 1);
			float const curve_segment_normalized_length = 1.0f / float(segment_count);
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				float curve_distance = normalized_distance / curve_segment_normalized_length;
				int segment = floor(curve_distance);
				float const s_u = fmodf(curve_distance, 1.0f);
				float const s_u2 = s_u * s_u;
				float const s_u3 = s_u * s_u2;

				glm::vec3 p0 = nodes[std::max(segment - 1, 0)];
				glm::vec3 p1 = nodes[segment];
				glm::vec3 p2 = nodes[segment + 1];
				glm::vec3 p3 = nodes[std::min(segment + 2, (int)nodes.size() - 1)];

				glm::vec3 const a = -p0 + 3.0f * p1 - 3.0f * p2 + p3;
				glm::vec3 const b = 2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3;
				glm::vec3 const c = -p0 + p2;
				glm::vec3 const d = 2.0f * p1;

				_lut->m_points[i] = 0.5f * (a * s_u3 + b * s_u2 + c * s_u + d);

				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}
		else if (_curve->m_type == piecewise_curve::EType::Hermite && nodes.size() >= 4)
		{
			// Only process 3 nodes at a time when using Hermite.
			// P0, P'0, P'1, P1 = P2, P'2, P'3, P3 = P4, etc...
			unsigned int const segment_count = (nodes.size() - 1) / 3;
			float const curve_segment_normalized_length = 1.0f / float(segment_count);
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				float curve_distance = normalized_distance / curve_segment_normalized_length;
				unsigned int segment = floor(curve_distance);
				float const s_u = fmodf(curve_distance, 1.0f);
				float const s_u2 = s_u * s_u;
				float const s_u3 = s_u * s_u2;

				unsigned int const segment_node_offset = 3 * segment;
				glm::vec3 const s_p0 = nodes[segment_node_offset];
				glm::vec3 const s_p0_t = nodes[segment_node_offset+1];
				glm::vec3 const s_p1_t = nodes[segment_node_offset+2];
				glm::vec3 const s_p1 = nodes[segment_node_offset+3];

				glm::vec3 const a = 2.0f * (s_p0 - s_p1) + s_p0_t + s_p1_t;
				glm::vec3 const b = 3.0f * (s_p1 - s_p0) - 2.0f * s_p0_t - s_p1_t;
				glm::vec3 const c = s_p0_t;
				glm::vec3 const d = s_p0;

				_lut->m_points[i] = a * s_u3 + b * s_u2 + c * s_u + d;

				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}
		else if (_curve->m_type == piecewise_curve::EType::Bezier && nodes.size() >= 4)
		{
			unsigned int const segment_count = (nodes.size() - 1) / 3;
			float const curve_segment_normalized_length = 1.0f / float(segment_count);
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				float curve_distance = normalized_distance / curve_segment_normalized_length;
				unsigned int segment = floor(curve_distance);
				float const s_u = fmodf(curve_distance, 1.0f);
				float const s_u2 = s_u * s_u;
				float const s_u3 = s_u * s_u2;
				float const s_inv_u = 1 - s_u;
				float const s_inv_u2 = s_inv_u * s_inv_u;
				float const s_inv_u3 = s_inv_u2 * s_inv_u;

				unsigned int const segment_node_offset = 3 * segment;
				glm::vec3 const s_p0 = nodes[segment_node_offset];
				glm::vec3 const s_p1 = s_p0 + nodes[segment_node_offset + 1];
				glm::vec3 const s_p3 = nodes[segment_node_offset + 3];
				glm::vec3 const s_p2 = s_p3 + nodes[segment_node_offset + 2];

				_lut->m_points[i] = s_inv_u3 * s_p0 + 3 * s_u * s_inv_u2 * s_p1 + 3 * s_u2 * s_inv_u * s_p2 + s_u3 * s_p3;

				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}

		// Generate distance part of LUT (brute force)
		//TODO: Use proper method describes in presentation.
		_lut->m_distances[0] = 0.0f;
		for (unsigned int i = 1; i < _curve->m_lut.m_points.size(); ++i)
			_lut->m_distances[i] = _lut->m_distances[i-1] + glm::distance(_lut->m_points[i], _lut->m_points[i - 1]);
	}

	piecewise_curve const& CurveInterpolator::GetPiecewiseCurve() const
	{
		return GetManager().m_map.at(m_owner);
	}
	void CurveInterpolator::SetPiecewiseCurve(piecewise_curve _curve, unsigned int _resolution)
	{
		piecewise_curve & curve = GetManager().m_map.at(m_owner);
		curve = _curve;
		GetManager().generate_curve_lut(&curve, &curve.m_lut, _resolution);
	}


	/*
	* Sets the type of the piecewise curve. Will attempt to reformat internal
	* node structure to preserve curve node positions.
	* @param	EType	Curve type
	*/
	void piecewise_curve::set_curve_type(EType _type)
	{
		// ### Curve has two node storage formats:
		// # Linear / Catmull: 
		// Each node is an explicit position on the curve.
		// # Hermite / Bezier
		// Only the 1st and 1+3n-th nodes are positions on the curve.
		// The nodes inbetween define the tangents corresponding to these positions.

		// Swiching between the above types should make sure to reduce the format properly, or to
		// provide a "correct" generated format.

		bool old_is_linear_or_catmull = (m_type == EType::Linear || m_type == EType::Catmull);
		bool new_is_linear_or_catmull = (_type == EType::Linear || _type == EType::Catmull);

		if (!m_nodes.empty() && old_is_linear_or_catmull != new_is_linear_or_catmull)
		{
			// Generate new curve along with default tangents / intermediate positions.
			if (old_is_linear_or_catmull)
			{
				std::vector<glm::vec3> generated_nodes(m_nodes.size() * 3 + 1, glm::vec3(0));
				generated_nodes[0] = m_nodes[0];
				if (_type == EType::Hermite)
				{
					for (unsigned int i = 1; i < m_nodes.size(); ++i)
					{
						glm::vec3 const dir_vec = glm::normalize(m_nodes[i] - m_nodes[i - 1]);
						generated_nodes[3 * i - 2] = dir_vec;
						generated_nodes[3 * i - 1] = dir_vec;
						generated_nodes[3 * i] = m_nodes[i];
					}
				}
				else if (_type == EType::Bezier)
				{
					for (unsigned int i = 1; i < m_nodes.size(); ++i)
					{
						glm::vec3 const halfway_point = (m_nodes[i] + m_nodes[i - 1])*0.5f;
						generated_nodes[3 * i - 2] = halfway_point;
						generated_nodes[3 * i - 1] = halfway_point;
						generated_nodes[3 * i] = m_nodes[i];
					}
				}
				m_nodes = std::move(generated_nodes);
			}
			// Reduce existing curve. Only take 3n-th points
			else
			{
				size_t const reduced_size = m_nodes.size() < 4 ? 0 : (m_nodes.size() - 1) / 3;
				std::vector<glm::vec3> generated_nodes(reduced_size, glm::vec3(0));
				for (unsigned int i = 0; i < generated_nodes.size(); ++i)
					generated_nodes[i] = m_nodes[3 * i];

				m_nodes = std::move(generated_nodes);
			}
		}
		m_type = _type;
	}
}