#include "Nameable.h"

namespace Component
{
	const char* NameableManager::GetComponentTypeName() const
	{
		return "Nameable";
	}

	void NameableManager::impl_clear()
	{
		m_entity_index_map.clear();
		m_index_entities.clear();
		m_index_names.clear();
	}

	bool NameableManager::impl_create(Entity _e)
	{
		unsigned int new_index = (unsigned int)m_index_names.size();
		m_entity_index_map.emplace(_e, new_index);
		m_index_entities.emplace_back(_e);
		m_index_names.emplace_back();
		return true;
	}

	void NameableManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			Entity const delete_entity = _entities[i];
			auto iter = m_entity_index_map.find(delete_entity);
			if (iter == m_entity_index_map.end())
				continue;
			
			// Swap deleted entity index with back index
			// Only bother if back entity is not deleted entity
			Entity const back_index_entity = m_index_entities.back();
			if (back_index_entity != delete_entity)
			{
				unsigned int const deleted_entity_index = iter->second;
				std::swap(m_index_names.at(deleted_entity_index), m_index_names.at(m_index_names.size() - 1));
				std::swap(m_index_entities[deleted_entity_index], m_index_entities.at(m_index_names.size() - 1));
				m_entity_index_map.at(m_index_entities.at(deleted_entity_index)) = deleted_entity_index;
			}
			m_entity_index_map.erase(delete_entity);
			m_index_entities.pop_back();
			m_index_names.pop_back();
		}
	}

	bool NameableManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_entity_index_map.find(_entity) != m_entity_index_map.end();
	}

	void NameableManager::impl_edit_component(Entity _entity)
	{
		static char name_buffer[MAX_STRING_SIZE];
		unsigned int const entity_index = m_entity_index_map.at(_entity);
		memcpy(name_buffer, &m_index_names[entity_index].front(), MAX_STRING_SIZE-1);
		if (ImGui::InputText("Name", name_buffer, MAX_STRING_SIZE))
			set_index_name(entity_index, name_buffer);
	}

	void NameableManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	uint8_t NameableManager::get_index_name_length(unsigned int _index) const
	{
		return MAX_STRING_SIZE - 1 - (uint8_t)m_index_names[_index].back();
	}

	uint8_t NameableManager::set_index_name_length(unsigned int _index, uint8_t _length)
	{
		assert(_length < MAX_STRING_SIZE);
		return m_index_names[_index].back() = MAX_STRING_SIZE - 1 - _length;
	}

	void NameableManager::set_index_name(unsigned int _index, const char* _name)
	{
		unsigned int input_name_string_length = (unsigned int)strlen(_name);
		memset(&m_index_names[_index].front(), 0, MAX_STRING_SIZE-1);
		memcpy(
			&m_index_names[_index].front(), _name, 
			std::min(input_name_string_length, MAX_STRING_SIZE-1)
		);
		set_index_name_length(_index, input_name_string_length);
	}

	const char* Nameable::GetName() const
	{
		return &GetManager().m_index_names[GetManager().m_entity_index_map.at(m_owner)].front();
	}

	const char* Nameable::SetName(const char * _name)
	{
		unsigned int const entity_index = GetManager().m_entity_index_map.at(m_owner);
		GetManager().set_index_name(entity_index, _name);
		return &GetManager().m_index_names[entity_index].front();
	}

	constexpr uint8_t Nameable::MaxNameLength()
	{
		return NameableManager::MAX_STRING_SIZE-1;
	}

}