#include "Light.h"
#include <Engine/Components/Transform.h>

namespace Component
{

	//////////////////////////////////////////////////////////////////
	//					Point Light Manager
	//////////////////////////////////////////////////////////////////

	bool PointLightManager::impl_create(Entity _e)
	{
		unsigned int const new_light_index = m_light_color_arr.size();
		m_entity_map.emplace(_e, new_light_index);
		m_light_color_arr.emplace_back(1.0f, 1.0f, 1.0f);
		m_light_radius_arr.emplace_back(10.0f);
		m_index_entities.emplace_back(_e);
		return true;
	}

	void PointLightManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			auto iter = m_entity_map.find(_entities[i]);
			if (iter != m_entity_map.end())
			{
				unsigned int const swap_index_to_back = iter->second;
				std::swap(m_light_color_arr[swap_index_to_back], m_light_color_arr.back());
				std::swap(m_light_radius_arr[swap_index_to_back], m_light_radius_arr.back());
				std::swap(m_index_entities[swap_index_to_back], m_index_entities.back());
				m_entity_map[m_index_entities[swap_index_to_back]] = swap_index_to_back;
				m_light_color_arr.pop_back();
				m_light_radius_arr.pop_back();
				m_index_entities.pop_back();
				m_entity_map.erase(iter);
			}
		}
	}

	bool PointLightManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_entity_map.find(_entity) != m_entity_map.end();
	}

	void PointLightManager::impl_edit_component(Entity _entity)
	{
		unsigned int const edit_index = m_entity_map.at(_entity);
		ImGui::ColorEdit3("Color", &m_light_color_arr[edit_index].x);
		ImGui::DragFloat("Radius", &m_light_radius_arr[edit_index], 1.0f, 0.1, 10000.0f, "%.1f");
	}

	void PointLightManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
		unsigned int const entity_index = m_entity_map.at(_e);
		m_light_color_arr[entity_index] = _json_comp["color"];
		m_light_radius_arr[entity_index] = _json_comp["radius"];
	}

	PointLightManager::Collection PointLightManager::GetPointLightCollection() const
	{
		Collection new_collection;
		new_collection.m_light_count = m_light_color_arr.size();
		if (!m_light_color_arr.empty())
		{
			new_collection.m_light_color_arr = &m_light_color_arr[0];
			new_collection.m_light_radius_arr = &m_light_radius_arr[0];
			new_collection.m_light_pos_arr.reserve(m_entity_map.size());
			for (auto entity : m_index_entities)
				new_collection.m_light_pos_arr.push_back(entity.GetComponent<Component::Transform>().ComputeWorldTransform().position);
		}
		else
		{
			new_collection.m_light_color_arr = nullptr;
			new_collection.m_light_radius_arr = nullptr;
			new_collection.m_light_pos_arr.clear();
		}
		return new_collection;
	}

	void PointLightManager::impl_clear()
	{
		m_entity_map.clear();
		m_light_color_arr.clear();
		m_light_radius_arr.clear();
		m_index_entities.clear();
	}

	float PointLight::GetRadius() const
	{
		return GetManager().m_light_radius_arr[get_light_index()];
	}

	void PointLight::SetRadius(float _radius)
	{
		GetManager().m_light_radius_arr[get_light_index()] = _radius;
	}

	glm::vec3 PointLight::GetColor() const
	{
		return GetManager().m_light_color_arr[get_light_index()];
	}

	void PointLight::SetColor(glm::vec3 _color)
	{
		GetManager().m_light_color_arr[get_light_index()] = _color;
	}

	size_t PointLight::get_light_index() const
	{
		return GetManager().m_entity_map.at(m_owner);
	}

}