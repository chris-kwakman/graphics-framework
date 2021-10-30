#include "CurveInterpolator.h"

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
		new_curve.m_type = piecewise_curve::EType::Linear;
		new_curve.m_nodes.clear();

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
		static int curve_resolution = 20;
		static int const MAX_POINTS_DISPLAYED = 7;
		static bool show_curve_point_list = true;
		static bool show_distance_lut_table = false;
		static Entity editing_entity = Entity::InvalidEntity;

		piecewise_curve& curve = m_map.at(_entity);

		bool rendering_curve = (m_renderable_curves.find(_entity) != m_renderable_curves.end());
		bool rendering_curve_nodes = (m_renderable_curve_nodes.find(_entity) != m_renderable_curve_nodes.end());

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
					curve.m_type = curr_type;
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

			for (unsigned int i = 0; i < curve.m_nodes.size(); ++i)
			{
				ImGui::PushID(i);
				char buffer[16];
				snprintf(buffer, sizeof(buffer), "###Point %i", i);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				if (ImGui::SmallButton("X"))
					delete_point_index = i;
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::DragFloat3(buffer, &curve.m_nodes[i].x, 0.01f, -FLT_MAX, FLT_MAX, "%.2f");
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

				normalized_distance += normalized_distance_delta;
			}
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

				normalized_distance += normalized_distance_delta;
			}
		}
		else if (_curve->m_type == piecewise_curve::EType::Bezier && nodes.size() >= 4)
		{
			//TODO: Implement Bezier
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
}