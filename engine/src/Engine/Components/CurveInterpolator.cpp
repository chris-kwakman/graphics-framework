
#include "CurveInterpolator.h"

#include <Engine/Editor/editor.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Transform.h>
#include <Engine/Utils/algorithm.h>

#include <ImGuizmo/ImGuizmo.h>
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

	decltype(CurveInterpolatorManager::m_renderable_curve_lut) const& CurveInterpolatorManager::GetRenderableCurveLUTs() const
	{
		return m_renderable_curve_lut;
	}

	void CurveInterpolatorManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_map = _j["m_map"].get<decltype(m_map)>();
			m_renderable_curves = _j["m_renderable_curves"].get<decltype(m_renderable_curves)>();
			m_renderable_curve_nodes = _j["m_renderable_curve_nodes"].get<decltype(m_renderable_curve_nodes)>();
			m_renderable_curve_lut = _j["m_renderable_curve_lut"].get<decltype(m_renderable_curve_lut)>();
		}
	}

	void CurveInterpolatorManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_map"] = m_map;
		_j["m_renderable_curves"] = m_renderable_curves;
		_j["m_renderable_curve_nodes"] = m_renderable_curve_nodes;
		_j["m_renderable_curve_lut"] = m_renderable_curve_lut;
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

		static int const MAX_POINTS_DISPLAYED = 7;
		static bool show_curve_point_list = true;
		static bool show_distance_lut_table = false;
		static Entity editing_entity = Entity::InvalidEntity;
		static bool generate_lut = false;

		piecewise_curve& curve = m_map.at(_entity);

		bool rendering_curve = (m_renderable_curves.find(_entity) != m_renderable_curves.end());
		bool rendering_curve_nodes = (m_renderable_curve_nodes.find(_entity) != m_renderable_curve_nodes.end());
		bool rendering_curve_lut = (m_renderable_curve_lut.find(_entity) != m_renderable_curve_lut.end());

		if (ImGui::Button("Copy JSON data to clipboard"))
		{
			nlohmann::json curve_json;
			nlohmann::json& nodes = curve_json["nodes"];
			nlohmann::json& type = curve_json["type"];
			nlohmann::json& adaptive = curve_json["adaptive"];

			adaptive = curve.m_lut.m_adaptive;
			if (curve.m_lut.m_adaptive)
			{
				nlohmann::json& tolerance = curve_json["tolerance"];
				nlohmann::json& max_subdivisions = curve_json["max_subdivisions"];

				tolerance = curve.m_lut.m_tolerance;
				max_subdivisions = (unsigned char)curve.m_lut.m_max_subdivisions;
			}
			else
			{
				nlohmann::json& resolution = curve_json["resolution"];
				resolution = curve.m_lut.m_arclengths.size();
			}

			nodes = curve.m_nodes;
			std::string type_name = std::string(s_curve_type_names.at(curve.m_type));
			std::transform(type_name.begin(), type_name.end(), type_name.begin(), std::tolower);
			type = type_name;

			ImGui::SetClipboardText(curve_json.dump(4).c_str());
		}

		auto mirror_curve_node = [&](int node_index)
		{
			// If we are editing curve w/ tangent / intermediate nodes (Hermite / Bezier),
			// adjust node values to maintain continuity.
			if (curve.m_type == curve_type::Hermite || curve.m_type == curve_type::Bezier)
			{
				float dir_mult = 1.0f;
				if (curve.m_type == curve_type::Bezier)
					dir_mult = -1.0f;

				int given_remainder = node_index % 3;
				if (given_remainder == 1 && node_index > 1)
				{
					float length = 0.0f;
					if (glm::all(glm::epsilonEqual(curve.m_nodes[node_index - 2], glm::vec3(0.0f), glm::epsilon<float>())))
					{
						curve.m_nodes[node_index - 2] = glm::vec3(0.0f);
					}
					else
					{
						length = glm::length(curve.m_nodes[node_index - 2]);
						curve.m_nodes[node_index - 2] = dir_mult * length * glm::normalize(curve.m_nodes[node_index]);
					}
				}
				else if (given_remainder == 2 && node_index < curve.m_nodes.size() - 2)
				{ 
					float length = 0.0f;
					if (glm::all(glm::epsilonEqual(curve.m_nodes[node_index + 2], glm::vec3(0.0f), glm::epsilon<float>())))
					{
						curve.m_nodes[node_index + 2] = glm::vec3(0.0f);
					}
					else
					{
						length = glm::length(curve.m_nodes[node_index + 2]);
						curve.m_nodes[node_index + 2] = dir_mult * length * glm::normalize(curve.m_nodes[node_index]);
					}
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
		if (ImGui::Checkbox("Render Curve LUT Points", &rendering_curve_lut))
		{
			if (rendering_curve_lut)
				m_renderable_curve_lut.emplace(_entity);
			else
				m_renderable_curve_lut.erase(_entity);
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
					generate_lut = true;
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::Checkbox("Adaptive LUT", &curve.m_lut.m_adaptive))
		{
			if (curve.m_lut.m_adaptive)
			{
				curve.m_lut.m_tolerance = 0.4f;
				curve.m_lut.m_max_subdivisions = 6;
			}
			else
			{
				curve.m_lut.m_resolution = 100;
			}
			generate_lut = true;
		}
		if (curve.m_lut.m_adaptive)
		{
			int casted_max_subdivisions = (int)curve.m_lut.m_max_subdivisions;
			if (ImGui::SliderInt("Max Subdivisions", &casted_max_subdivisions, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				curve.m_lut.m_max_subdivisions = (unsigned int)casted_max_subdivisions;
				generate_lut = true;
			}
			float copied_tolerance = curve.m_lut.m_tolerance;
			if (ImGui::SliderFloat("Tolerance", &copied_tolerance, 0.00001f, 1.0f, "%.4f"))
			{
				curve.m_lut.m_tolerance = copied_tolerance;
				generate_lut = true;
			}
		}
		else
		{
			int casted_resolution = (int)curve.m_lut.m_resolution;
			if (ImGui::SliderInt("Curve Resolution", &casted_resolution, 2, 200, "%d", ImGuiSliderFlags_AlwaysClamp))
				curve.m_lut.m_resolution = (unsigned int)casted_resolution;
		}
		if (ImGui::Button("Regenerate LUT"))
			generate_lut = true;

		ImGui::Checkbox("Show Curve Node List", &show_curve_point_list);
		ImGui::Checkbox("Show Distance LUT", &show_distance_lut_table);
		if (ImGui::Button("Add Curve Node"))
		{
			if(curve.m_nodes.empty())
				curve.m_nodes.push_back(glm::vec3(0.0f));
			else
				curve.m_nodes.push_back(curve.m_nodes.back());

			generate_lut = true;
		}

		// Debugging tool for testing whether normalized parameter computation works.
		static float s_input_arclength = 0.0f;
		static float s_output_parameter = 0.0f;
		if (ImGui::SliderFloat(
			"Get Normalized Param", 
			&s_input_arclength, 
			0.0f, 
			curve.m_lut.m_arclengths.empty() ? 0.0f : curve.m_lut.m_arclengths.back(), 
			"%.2f"
		))
			s_output_parameter = curve.m_lut.compute_normalized_parameter(s_input_arclength);
		ImGui::SameLine();
		ImGui::Text("%.3f", s_output_parameter);

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
				Component::Transform const edit_entity_transform = _entity.GetComponent<Component::Transform>();
				node_position += edit_entity_transform.ComputeWorldTransform().position;

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
					translation -= edit_entity_transform.ComputeWorldTransform().position;

					curve.m_nodes[transform_node_index] = translation;

					mirror_curve_node(transform_node_index);

					generate_lut = true;
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
					generate_lut = true;
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
			{
				curve.m_nodes.erase(curve.m_nodes.begin() + delete_point_index);
				generate_lut = true;
			}
			if (move_point_index != -1)
			{
				std::swap(
					curve.m_nodes.at(move_point_index), 
					curve.m_nodes.at(move_point_index + (int)!move_up - (int)move_up)
				);
				generate_lut = true;
			}
			ImGui::EndListBox();
		}
		if (show_distance_lut_table)
		{
			ImGuiTableFlags const table_flags =
				ImGuiTableFlags_Hideable |
				ImGuiTableFlags_Reorderable |
				ImGuiTableFlags_ScrollY |
				ImGuiTableFlags_Borders;
			unsigned int const TABLE_COLUMN_COUNT = (curve.m_lut.m_normalized_parameters.empty() ? 3 : 4);
			if (ImGui::BeginTable("Distance LUT", TABLE_COLUMN_COUNT, table_flags, ImVec2(ImGui::GetWindowWidth(), 400.0f)))
			{
				for (unsigned int i = 0; i < curve.m_lut.m_arclengths.size(); ++i)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%i", i);

					ImGui::TableNextColumn();
					glm::vec3 const point = curve.m_lut.m_points[i];
					ImGui::Text("%.2f, %.2f, %.2f", point.x, point.y, point.z);

					if (!curve.m_lut.m_normalized_parameters.empty())
					{
						ImGui::TableNextColumn();
						ImGui::Text("%.3f", curve.m_lut.m_normalized_parameters[i]);
					}
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", curve.m_lut.m_arclengths[i]);
				}
				ImGui::EndTable();
			}
		}

		if (generate_lut)
		{
			generate_lut = false;
			if (curve.m_lut.m_adaptive)
				generate_curve_lut_adaptive(&curve, &curve.m_lut, curve.m_lut.m_max_subdivisions, curve.m_lut.m_tolerance);
			else
				generate_curve_lut(&curve, &curve.m_lut, curve.m_lut.m_resolution);
		}
	}

	void CurveInterpolatorManager::generate_curve_lut(piecewise_curve const* _curve, lookup_table* _lut, unsigned int _lut_resolution)
	{
		assert(_curve);
		assert(_lut);

		_lut->m_resolution = _lut_resolution;

		_lut->m_arclengths.clear();
		_lut->m_points.clear();
		_lut->m_normalized_parameters.clear();

		if (_curve->m_type == curve_type::Linear)
		{
			_lut->m_arclengths.resize(_curve->m_nodes.size());
			_lut->m_points.resize(_curve->m_nodes.size());
			_lut->m_normalized_parameters.resize(_curve->m_nodes.size());
		}
		else
		{
			_lut->m_arclengths.resize(_lut_resolution);
			_lut->m_points.resize(_lut_resolution);
			_lut->m_normalized_parameters.resize(_lut_resolution);
		}

		std::vector<glm::vec3> const& nodes = _curve->m_nodes;

		float const normalized_distance_delta = 1.0f / float(_lut_resolution-1);
		float normalized_distance = 0.0f;
		if (_curve->m_type == piecewise_curve::EType::Linear)
		{
			_lut->m_points = nodes;
			for (unsigned int i = 0; i < _curve->m_nodes.size(); ++i)
				_lut->m_normalized_parameters[i] = (float)i / float(_curve->m_nodes.size() - 1u);
		}
		else if (_curve->m_type == piecewise_curve::EType::Catmull && nodes.size() >= 2)
		{
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				_lut->m_points[i] = _curve->numerical_sample_catmull(normalized_distance);
				_lut->m_normalized_parameters[i] = normalized_distance;
				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}
		else if (_curve->m_type == piecewise_curve::EType::Hermite && nodes.size() >= 4)
		{
			// Only process 3 nodes at a time when using Hermite.
			// P0, P'0, P'1, P1 = P2, P'2, P'3, P3 = P4, etc...
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				_lut->m_points[i] = _curve->numerical_sample_hermite(normalized_distance);
				_lut->m_normalized_parameters[i] = normalized_distance;
				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}
		else if (_curve->m_type == piecewise_curve::EType::Bezier && nodes.size() >= 4)
		{
			for (unsigned int i = 0; i < _lut_resolution; ++i)
			{
				_lut->m_points[i] = _curve->numerical_sample_bezier(normalized_distance);
				_lut->m_normalized_parameters[i] = normalized_distance;
				normalized_distance = std::clamp(normalized_distance + normalized_distance_delta, 0.0f, 1.0f);
			}
		}

		// Generate distance part of LUT (brute force)
		_lut->m_arclengths[0] = 0.0f;
		for (unsigned int i = 1; i < _curve->m_lut.m_points.size(); ++i)
			_lut->m_arclengths[i] = _lut->m_arclengths[i-1] + glm::distance(_lut->m_points[i], _lut->m_points[i - 1]);
	}

	/*
	* Applies adaptive forward differencing to curve to generate LUT.
	* @param	piecewise_curve *	Curve that will be sampled
	* @param	lookup_table *		Lookup table to adaptively reduce
	* @param	unsigned int		Level of subdivision that will be applied to curve.
	* @param	float				Treshhold
	*/
	void CurveInterpolatorManager::generate_curve_lut_adaptive(
		piecewise_curve const* _curve, lookup_table* _lut, 
		unsigned int _subdivisions, float _tolerance
	)
	{
		assert(_curve);
		assert(_lut);

		_lut->m_adaptive = true;
		_lut->m_max_subdivisions = _subdivisions;
		_lut->m_tolerance = _tolerance;

		_lut->m_arclengths.clear();
		_lut->m_points.clear();
		_lut->m_normalized_parameters.clear();

		if (_curve->m_type == piecewise_curve::EType::Linear)
			generate_curve_lut(_curve, _lut, (unsigned int)_curve->m_nodes.size());
		else
		{
			_lut->m_arclengths.resize(1);
			_lut->m_normalized_parameters.resize(1);
			_lut->m_points.resize(1);

			_lut->m_normalized_parameters[0] = 0.0f;
			_lut->m_arclengths[0] = 0.0f;
			_lut->m_points[0] = _curve->m_nodes[0];

			rec_adaptive_forward_differencing(
				_curve, _lut, 0.0f, 1.0f, _subdivisions, _tolerance
			);
		}
	}

	/*
	* Helper function to recursively sample curves
	* @param	piecewise_curve *	Curve to sample
	* @param	float				Normalized parameter of front of segment
	* @param	float				Normalized parameter of back of segment.
	* @param	int					Remaining subdivisions
	* @param	float				Tolerance for accepting early breaks.
	*/
	void CurveInterpolatorManager::rec_adaptive_forward_differencing(
		piecewise_curve const* _curve, 
		lookup_table * _lut,
		float _u_left, float _u_right, 
		int _remaining_subdivisions, float _tolerance
	)
	{
		float const u_middle = _u_left + (_u_right - _u_left) * 0.5f;
		float length_left, length_right, length_total;
		glm::vec3 point_left, point_right, point_middle;

		point_left = _curve->numerical_sample(_u_left);
		point_middle = _curve->numerical_sample(u_middle);
		length_left = glm::distance(point_left, point_middle);

		if (_remaining_subdivisions >= 0)
		{
			// Test if distance between left segment, right segment and direct distance is below tolerance.
			point_right = _curve->numerical_sample(_u_right);
			length_right = glm::distance(point_right, point_middle);
			length_total = glm::distance(point_left, point_right);

			if (glm::abs((length_left + length_right) - length_total) < _tolerance)
			{
				_lut->m_arclengths.push_back(_lut->m_arclengths.back() + length_total);
				_lut->m_points.push_back(point_right);
				_lut->m_normalized_parameters.push_back(_u_right);
			}
			else
			{
				rec_adaptive_forward_differencing(
					_curve, _lut,
					_u_left, u_middle,
					_remaining_subdivisions - 1,
					_tolerance * 0.5f
				);
				rec_adaptive_forward_differencing(
					_curve, _lut,
					u_middle, _u_right,
					_remaining_subdivisions - 1,
					_tolerance * 0.5f
				);
			}
		}
		else
		{
			_lut->m_arclengths.push_back(_lut->m_arclengths.back() + length_left);
			_lut->m_points.push_back(point_middle);
			_lut->m_normalized_parameters.push_back(u_middle);
		}
	}

	

	piecewise_curve const& CurveInterpolator::GetPiecewiseCurve() const
	{
		return GetManager().m_map.at(m_owner);
	}

	/*
	* Set piecewise curve and generate regular LUT
	* @param	piecewise_curve			Reference curve
	* @param	unsigned int			Resolution of generated LUT (i.e. # of samples)
	*/
	void CurveInterpolator::SetPiecewiseCurve(piecewise_curve _curve, unsigned int _resolution)
	{
		piecewise_curve & curve = GetManager().m_map.at(m_owner);
		curve = _curve;
		GetManager().generate_curve_lut(&curve, &curve.m_lut, _resolution);
	}

	/*
	* Set piecewise curve and generate adaptive LUT.
	* @param	piecewise_curve			Reference curve
	* @param	float					Adaptive forward differencing tolerance
	* @param	unsigned int			Max number of subdivisions when checking for tolerance
	*/
	void CurveInterpolator::SetPiecewiseCurve(piecewise_curve _curve, float _tolerance, unsigned int _max_subdivisions)
	{
		piecewise_curve& curve = GetManager().m_map.at(m_owner);
		curve = _curve;
		GetManager().generate_curve_lut_adaptive(&curve, &curve.m_lut, _max_subdivisions, _tolerance);
	}

	/*
	* Get position on curve in space of owning entity
	* @param	float		Distance parameter
	* @returns	glm::vec3	Position on curve in space of owning entity
	*/
	glm::vec3 CurveInterpolator::GetPosition(float _distance) const
	{
		Component::Transform owner_transform = Owner().GetComponent<Component::Transform>();
		return owner_transform.ComputeWorldTransform().position + GetPiecewiseCurve().m_lut.get_distance_position(_distance);
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

	glm::vec3 piecewise_curve::numerical_sample(float _param) const
	{
		switch (m_type)
		{
		case EType::Linear: return numerical_sample_linear(_param); 
		case EType::Catmull: return numerical_sample_catmull(_param);
		case EType::Hermite: return numerical_sample_hermite(_param);
		case EType::Bezier: return numerical_sample_bezier(_param);
		}
		return glm::vec3(0.0f);
	}

	struct setup_sample_output
	{
		int left;
		int right;
		float local_segment_param;
	};

	static setup_sample_output setup_sample_contiguous_curve(float _param, unsigned int _point_count)
	{
		setup_sample_output new_sso;

		float const FLT_SEGMENT_COUNT = (float)_point_count - 1.0f;
		float const param_sample_segment = _param * FLT_SEGMENT_COUNT;
		new_sso.local_segment_param = fmodf(param_sample_segment, 1.0f);
		new_sso.left = (int)floorf(param_sample_segment);
		new_sso.right = (int)ceilf(param_sample_segment);
		return new_sso;
	}

	glm::vec3 piecewise_curve::numerical_sample_linear(float _param) const
	{
		auto result = setup_sample_contiguous_curve(_param, (unsigned int)m_nodes.size());
		return	(1.0f - result.local_segment_param) * m_nodes[result.left] +
			result.local_segment_param * m_nodes[result.right];
	}

	glm::vec3 piecewise_curve::numerical_sample_catmull(float _param) const
	{
		auto result = setup_sample_contiguous_curve(_param, (unsigned int)m_nodes.size());

		float const s_u = result.local_segment_param;
		float const s_u2 = s_u * s_u;
		float const s_u3 = s_u * s_u2;

		glm::vec3 const p0 = m_nodes[std::max(result.left - 1, 0)];
		glm::vec3 const p1 = m_nodes[result.left];
		glm::vec3 const p2 = m_nodes[result.right];
		glm::vec3 const p3 = m_nodes[std::min(result.right + 1, (int)m_nodes.size() - 1)];

		glm::vec3 const a = -p0 + 3.0f * p1 - 3.0f * p2 + p3;
		glm::vec3 const b = 2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3;
		glm::vec3 const c = -p0 + p2;
		glm::vec3 const d = 2.0f * p1;

		return 0.5f * (a * s_u3 + b * s_u2 + c * s_u + d);
	}

	glm::vec3 piecewise_curve::numerical_sample_hermite(float _param) const
	{
		// Number of points in the curve (NOT nodes)
		unsigned int const POINT_COUNT = 1 + ((unsigned int)m_nodes.size() - 1u) / 3u;
		auto result = setup_sample_contiguous_curve(_param, POINT_COUNT);

		unsigned int const segment = (unsigned int)floorf(_param * (float)(POINT_COUNT - 1));
		float const s_u = result.local_segment_param;
		float const s_u2 = s_u * s_u;
		float const s_u3 = s_u * s_u2;

		unsigned int const segment_node_offset = 3 * segment;
		glm::vec3 const s_p0 =		m_nodes[segment_node_offset];
		glm::vec3 const s_p0_t =	m_nodes[segment_node_offset + 1];
		glm::vec3 const s_p1_t =	m_nodes[segment_node_offset + 2];
		glm::vec3 const s_p1 =		m_nodes[segment_node_offset + 3];

		glm::vec3 const a = 2.0f * (s_p0 - s_p1) + s_p0_t + s_p1_t;
		glm::vec3 const b = 3.0f * (s_p1 - s_p0) - 2.0f * s_p0_t - s_p1_t;
		glm::vec3 const c = s_p0_t;
		glm::vec3 const d = s_p0;

		return a * s_u3 + b * s_u2 + c * s_u + d;
	}

	glm::vec3 piecewise_curve::numerical_sample_bezier(float _param) const
	{
		// Number of points in the curve (NOT nodes)
		unsigned int const POINT_COUNT = 1u + ((unsigned int)m_nodes.size() - 1u) / 3u;
		auto result = setup_sample_contiguous_curve(_param, POINT_COUNT);

		unsigned int const segment = (unsigned int)(_param * (float)(POINT_COUNT - 1u));
		float const s_u = result.local_segment_param;
		float const s_u2 = s_u * s_u;
		float const s_u3 = s_u * s_u2;
		float const s_inv_u = 1 - s_u;
		float const s_inv_u2 = s_inv_u * s_inv_u;
		float const s_inv_u3 = s_inv_u2 * s_inv_u;

		unsigned int const segment_node_offset = 3 * segment;
		glm::vec3 const s_p0 = m_nodes[segment_node_offset];
		glm::vec3 const s_p1 = s_p0 + m_nodes[segment_node_offset + 1u];
		glm::vec3 const s_p3 = m_nodes[segment_node_offset + 3u];
		glm::vec3 const s_p2 = s_p3 + m_nodes[segment_node_offset + 2u];

		return s_inv_u3 * s_p0 + 3 * s_u * s_inv_u2 * s_p1 + 3 * s_u2 * s_inv_u * s_p2 + s_u3 * s_p3;
	}

	/*
	* Compute normalized parameter given an index in the lookup table
	* Curve is sampled at uniform parameter interval.
	* @param	unsigned int		Index in lookup table
	* @returns	float				Normalized parameter value within curve.
	*/
	float lookup_table::get_normalized_parameter(unsigned int _index) const
	{
		return (float)_index / (float)((unsigned int)m_points.size() - 1u);
	}

	/*
	* Compute distance given normalized parameter from the start of the curve.
	* @param	float				Normalized parameter within curve [0.0f,1.0f]
	* @details						Performs binary search in LUT to find
	*								nearest arclength values, and interpolates
	*								between those corresponding arclength 
	*								values to get appropriate arclength.
	*/
	float lookup_table::compute_arclength(float _normalized_param) const
	{
		int idx_min = 0;
		int idx_max = (int)m_points.size() - 1;
		if (m_points.size() < 2)
			return 0.0f;
		else if (m_points.size() < 3)
		{
			idx_min = 0;
			idx_max = 1;
		}
		else
		{
			// Binary search to find bracketing indices whose values contain given parameters.
			auto bracket_pair = Engine::Utils::float_binary_search(&m_normalized_parameters.front() + idx_min, (idx_max - idx_min) + 1, _normalized_param);
			idx_min = bracket_pair.first;
			idx_max = bracket_pair.second;
		}

		float offset = _normalized_param - m_normalized_parameters[idx_min];
		float bracket_range = m_normalized_parameters[idx_max] - m_normalized_parameters[idx_min];
		float segment_param = std::clamp(offset / bracket_range, 0.0f, 1.0f);
		return m_arclengths[idx_min] * (1.0f - segment_param) + m_arclengths[idx_max] * segment_param;
	}

	/*
	* Compute normalized parameter given arclength from the start of the curve.
	* @param	float				Arclength from start of curve
	* @returns	float				Normalized parameter value within curve.
	* @details						Performs binary search in LUT to find
	*								nearest arclength values, and interpolates
	*								between those corresponding normalized parameter
	*								values to get appropriate normalized parameter.
	*/
	float lookup_table::compute_normalized_parameter(float _arclength) const
	{
		int idx_min = 0;
		int idx_max = (int)m_points.size()-1;

		if (m_points.size() < 2)
			return 0.0f;
		else if (m_points.size() < 3)
		{
			idx_min = 0;
			idx_max = 1;
		}
		else
		{
			// Binary search to find bracketing indices whose values contain given arclength.
			auto bracket_pair = Engine::Utils::float_binary_search(&m_arclengths.front() + idx_min, (idx_max - idx_min) + 1, _arclength);
			idx_min = bracket_pair.first;
			idx_max = bracket_pair.second;
		}

		float offset = _arclength - m_arclengths[idx_min];
		float bracket_range = m_arclengths[idx_max] - m_arclengths[idx_min];
		float segment_param = std::clamp(offset / bracket_range, 0.0f, 1.0f);
		return m_normalized_parameters[idx_min] * (1.0f - segment_param)
			+ m_normalized_parameters[idx_max] * segment_param;
	}

	glm::vec3 lookup_table::get_distance_position(float _arclength) const
	{
		int idx_min = 0;
		int idx_max = (int)m_points.size() - 1;

		if (m_points.size() < 2)
			return glm::vec3(0.0f);
		else if (m_points.size() < 3)
		{
			idx_min = 0;
			idx_max = 1;
		}
		else
		{
			// Binary search to find bracketing indices whose values contain given arclength.
			do
			{
				int idx_mid = (idx_max + idx_min) / 2;
				if (_arclength < m_arclengths[idx_mid])
					idx_max = idx_mid;
				else
					idx_min = idx_mid;
			} while (idx_max - idx_min > 1);
		}

		float offset = _arclength - m_arclengths[idx_min];
		float bracket_range = m_arclengths[idx_max] - m_arclengths[idx_min];
		float segment_param = std::clamp(offset / bracket_range, 0.0f, 1.0f);
		
		return (1.0f - segment_param) * m_points[idx_min] + segment_param * m_points[idx_max];
	}
}