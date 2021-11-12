#include "CurveFollower.h"
#include "Transform.h"
#include <ImGui/imgui_stdlib.h>

namespace Component
{
	const char* CurveFollowerManager::GetComponentTypeName() const
	{
		return "CurveFollower";
	}

	void CurveFollowerManager::UpdateFollowers(float _dt)
	{
		std::vector<Entity> remove_from_updateable;
		// Animate positions
		for (Entity animateable_entity : m_animateable)
		{
			follower_data & update_data = m_follower_map.at(animateable_entity);

			float arclength_travelled = 0.0f;
			// Update distance depending on used distance-time function
			if (update_data.m_distance_time_func == follower_data::eLinear)
			{
				arclength_travelled = update_data.m_lin_dist_time_data.m_travel_rate * _dt;
			}
			else
			{
				auto const& curve_lut = update_data.m_curve_component.GetPiecewiseCurve().m_lut;
				float current_param = curve_lut.compute_normalized_parameter(update_data.m_arclength);
				float new_param = current_param + update_data.m_sine_interp_dist_time_data.m_travel_rate * _dt;

				float const PI = glm::pi<float>();
				float const t1 = update_data.m_sine_interp_dist_time_data.m_seg_front_param;
				float const t2 = update_data.m_sine_interp_dist_time_data.m_seg_back_param;
				auto s1 = [PI, t1](float _t)->float
				{
					return t1 * 2.0f / PI * (1.0f + sinf((PI * 0.5f) * ((_t / t1) - 1.0f)));
				};
				auto s2 = [PI, t1](float _t)->float
				{
					return (_t - t1) + (2 * t1) / PI;
				};
				auto s3 = [PI, t1, t2](float _t)->float
				{
					return (1 - t2) * (2 / PI) * sin(((_t - t2) / (1 - t2)) * (PI * 0.5f));
				};
				float const f = (2 * t1 / PI) + t2 - t1 + 2 * (1 - t2) / PI;
				float new_arclength = 0.0f;

				bool recompute_param = false;
				do
				{
					recompute_param = false;

					if (new_param <= t1)
						new_param = s1(std::clamp(new_param, 0.0f, t1)) / f;
					else if (new_param >= 1.0f - t2)
						new_param = s3(std::clamp(new_param, t2, 1.0f)) / f;
					else
						new_param = s2(new_param) / f;

					if (new_param < 0.0f)
					{
						new_param += 1.0f;
						recompute_param = true;
					}
					else if (new_param > 1.0f)
					{
						new_param -= 1.0f;
						recompute_param = true;
					}
				} while (recompute_param);
				new_arclength = update_data.m_curve_component.GetPiecewiseCurve().m_lut.compute_arclength(new_param);
				arclength_travelled = new_arclength - update_data.m_arclength;
			}

			// Make sure travel rate does not exceed maximum set.
			if (update_data.m_max_travel_rate != 0.0f && arclength_travelled != 0.0f)
			{
				float const max_arclength_travelled = update_data.m_max_travel_rate * _dt;
				if (std::abs(arclength_travelled) > max_arclength_travelled)
					arclength_travelled *= max_arclength_travelled / std::abs(arclength_travelled);
			}
			float new_arclength = update_data.m_arclength + arclength_travelled;

			float const MAX_CURVE_ARCLENGTH = update_data.m_curve_component.GetPiecewiseCurve().m_lut.m_arclengths.back();
			if (update_data.m_loop)
			{
				while(new_arclength < 0.0f)
					new_arclength += MAX_CURVE_ARCLENGTH;
				while(new_arclength > MAX_CURVE_ARCLENGTH)
					new_arclength -= MAX_CURVE_ARCLENGTH;
			}
			else
			{
				if (new_arclength < 0.0f)
				{
					remove_from_updateable.emplace_back(animateable_entity);
				}
				else if (new_arclength > MAX_CURVE_ARCLENGTH)
				{
					remove_from_updateable.emplace_back(animateable_entity);
				}
			}
			update_data.m_arclength = std::clamp(new_arclength, 0.0f, MAX_CURVE_ARCLENGTH);
		}
		// Update positions of following entities.
		for (auto pair : m_follower_map)
		{
			if (pair.second.m_curve_component.IsValid())
			{
				glm::vec3 const curve_world_position = pair.second.m_curve_component.Owner().GetComponent<Component::Transform>().ComputeWorldTransform().position;
				pair.first.GetComponent<Component::Transform>().SetLocalPosition(
					curve_world_position + pair.second.m_curve_component.GetPiecewiseCurve().m_lut.get_distance_position(pair.second.m_arclength)
				);
			}
		}
	}

	void CurveFollowerManager::set_follower_playing_state(Entity _e, bool _play_state)
	{
		follower_data& data = m_follower_map.at(_e);
		bool is_playing = m_animateable.find(_e) != m_animateable.end();
		if (_play_state && data.m_curve_component.IsValid())
		{
			float const MAX_CURVE_ARCLENGTH = data.m_curve_component.GetPiecewiseCurve().m_lut.m_arclengths.back();
			if (data.m_lin_dist_time_data.m_travel_rate < 0.0f && data.m_arclength < 0.0f)
				data.m_arclength = data.m_arclength + MAX_CURVE_ARCLENGTH;
			else if (data.m_lin_dist_time_data.m_travel_rate > 0.0f && data.m_arclength > MAX_CURVE_ARCLENGTH)
				data.m_arclength = data.m_arclength - MAX_CURVE_ARCLENGTH;
			m_animateable.insert(_e);
		}
		else
			m_animateable.erase(_e);

	}

	void CurveFollowerManager::impl_clear()
	{
		m_follower_map.clear();
		m_animateable.clear();
	}

	bool CurveFollowerManager::impl_create(Entity _e)
	{
		follower_data new_follower_data;
		new_follower_data.set_linear_dist_time_func_data(1.0f);
		new_follower_data.m_arclength = 0.0f;
		new_follower_data.m_max_travel_rate = 1.0f;
		new_follower_data.m_loop = true;
		new_follower_data.m_curve_component = Entity::InvalidEntity;
		m_follower_map.emplace(_e, std::move(new_follower_data));
		// set_follower_playing_state(_e, false); // Implied
		return true;
	}

	void CurveFollowerManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			m_follower_map.erase(_entities[i]);
			m_animateable.erase(_entities[i]);
		}
	}

	bool CurveFollowerManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_follower_map.find(_entity) != m_follower_map.end();
	}

	void CurveFollowerManager::impl_edit_component(Entity _entity)
	{
		follower_data& data = m_follower_map.at(_entity);

		bool is_playing = m_animateable.find(_entity) != m_animateable.end();
		if (ImGui::Checkbox("Is Playing", &is_playing))
		{
			set_follower_playing_state(_entity, is_playing);
		}
		bool casted_loop = data.m_loop;
		if (ImGui::Checkbox("Is Looping", &casted_loop))
			data.m_loop = casted_loop;

		std::string curve_component_entity_name;
		if (data.m_curve_component.IsValid())
			curve_component_entity_name = data.m_curve_component.Owner().GetName();
		else
			curve_component_entity_name = "No Curve Set";
		ImGui::InputText("Follow Entity Curve", &curve_component_entity_name, ImGuiInputTextFlags_ReadOnly);
		if (ImGui::BeginDragDropTarget())
		{
			if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("ENTITY"))
			{
				Entity payload_entity = *(Entity*)payload->Data;
				CurveInterpolator curve_component = payload_entity.GetComponent<CurveInterpolator>();
				if (curve_component.IsValid())
					data.m_curve_component = curve_component;
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Drag-Drop Entity with CurveInterpolator component from scene graph into this box to make this component follow the curve of the dragged entity.");
			ImGui::EndTooltip();
		}

		float curve_max_distance = 0.0f;
		if (!data.m_curve_component.IsValid())
			ImGui::BeginDisabled();
		else
			curve_max_distance = data.m_curve_component.GetPiecewiseCurve().m_lut.m_arclengths.back();

		ImGui::SliderFloat("Distance along curve", &data.m_arclength, 0.0f, curve_max_distance, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Maximum Travel Rate", &data.m_max_travel_rate, 0.05f, 0.0f, FLT_MAX);

		static std::unordered_map<follower_data::distance_time_func, const char*> s_dist_time_func_names{
			{follower_data::eLinear, "Linear"},
			{follower_data::eSineInterpolation, "Sine Easing"}
		};
		if (ImGui::BeginCombo("Distance-Time Function", s_dist_time_func_names.at(data.m_distance_time_func)))
		{
			auto iter = s_dist_time_func_names.begin();
			while (iter != s_dist_time_func_names.end())
			{
				bool const option_is_selected = iter->first == data.m_distance_time_func;
				if (ImGui::Selectable(iter->second, &option_is_selected))
				{
					if (iter->first == follower_data::eLinear)
					{
						float const arclength_travel_rate = data.m_sine_interp_dist_time_data.m_travel_rate * data.m_curve_component.GetPiecewiseCurve().m_lut.m_arclengths.back();
						data.set_linear_dist_time_func_data(arclength_travel_rate);
					}
					else if (iter->first == follower_data::eSineInterpolation)
					{
						float const param_travel_rate = data.m_lin_dist_time_data.m_travel_rate / data.m_curve_component.GetPiecewiseCurve().m_lut.m_arclengths.back();
						data.set_sine_interp_dist_time_func_data(param_travel_rate, 0.25f, 0.25f);
					}
				}
				++iter;
			}
			ImGui::EndCombo();
		}

		if (data.m_distance_time_func == follower_data::eLinear)
		{
			ImGui::DragFloat("Travel Rate", &data.m_lin_dist_time_data.m_travel_rate, 0.05f, -FLT_MAX, FLT_MAX);
		}
		else if(data.m_distance_time_func == follower_data::eSineInterpolation)
		{
			auto& sin_dist_time_data = data.m_sine_interp_dist_time_data;
			ImGui::DragFloat("Param Length Front Segment", &sin_dist_time_data.m_seg_front_param, 0.01f, 0.0f, 1.0f - sin_dist_time_data.m_seg_front_param);
			ImGui::DragFloat("Param Length Back Segment", &sin_dist_time_data.m_seg_back_param, 0.01f, 0.0f, 1.0f - sin_dist_time_data.m_seg_front_param);
		}

		if (!data.m_curve_component.IsValid())
			ImGui::EndDisabled();
	}

	void CurveFollowerManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	///////////////////////////////////////////////////
	//			Curve Follower Component
	///////////////////////////////////////////////////

	follower_data& CurveFollower::GetFollowerData()
	{
		return GetManager().m_follower_map.at(Owner());
	}
	bool CurveFollower::GetPlayingState() const
	{
		return GetManager().m_animateable.find(Owner()) != GetManager().m_animateable.end();
	}
	void CurveFollower::SetPlayingState(bool _state)
	{
		GetManager().set_follower_playing_state(Owner(), _state);
	}
	void follower_data::set_linear_dist_time_func_data(float _travel_rate)
	{
		m_distance_time_func = follower_data::eLinear;
		m_lin_dist_time_data.m_travel_rate = _travel_rate;
	}
	void follower_data::set_sine_interp_dist_time_func_data(float _param_travel_rate, float _seg_front_param, float _seg_back_param)
	{
		float total_sine_portion = _seg_front_param + _seg_back_param;
		if (total_sine_portion > 1.0f)
		{
			_seg_front_param = _seg_front_param / total_sine_portion;
			_seg_back_param = _seg_back_param / total_sine_portion;
		}
		m_distance_time_func = follower_data::eSineInterpolation;
		m_sine_interp_dist_time_data.m_seg_front_param = std::clamp(_seg_front_param, 0.0f, 1.0f);
		m_sine_interp_dist_time_data.m_seg_back_param = std::clamp(_seg_back_param, 0.0f, 1.0f);
		m_sine_interp_dist_time_data.m_travel_rate = _param_travel_rate;
	}
}
