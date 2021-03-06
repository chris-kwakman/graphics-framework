#include "entity.h"
#include <Engine/Utils/singleton.h>
#include <Engine/Serialisation/common.h>
#include <Engine/Components/Nameable.h>

#include <Engine/Serialisation/compress.h>

#include "component_manager.h"

#include <cassert>

namespace Engine {
namespace ECS {

	Entity const Entity::InvalidEntity;

	typedef unsigned long long compacted_type;
	unsigned int const compacted_type_bits = sizeof(compacted_type) * 8;

	void EntityManager::Deserialize(nlohmann::json const& _j)
	{
		using namespace Engine::Serialisation;
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			std::vector<bool> transformed_bit_set = _j["m_entity_in_use_flag"];
			for (size_t i = 0; i < m_entity_in_use_flag.size(); i++)
			{
				m_entity_in_use_flag[i] = transformed_bit_set[i];
			}
			m_entity_counters = _j["m_entity_counters"];
			m_entity_id_iter = _j["m_entity_id_iter"];
		}
		if (serializer_version == 2)
		{
			{
				auto compressed_use_flags = _j["m_entity_in_use_flag"].get<std::vector<compressed_type>>();
				std::vector<bool> decompressed_use_flags = decompress_data_vector<bool>(compressed_use_flags, m_entity_in_use_flag.size());
				for (size_t i = 0; i < m_entity_in_use_flag.size(); i++)
					m_entity_in_use_flag[i] = decompressed_use_flags[i];
			}
			{
				auto compressed_counters = _j["m_entity_counters"].get<std::vector<compressed_type>>();
				auto decompressed_counters = decompress_data_vector<decltype(m_entity_counters)::value_type>(compressed_counters, m_entity_counters.size());
				for (size_t i = 0; i < m_entity_counters.size(); i++)
					m_entity_counters[i] = decompressed_counters[i];
			}

			m_entity_id_iter = _j["m_entity_id_iter"];
		}
	}

	void EntityManager::Serialize(nlohmann::json& _j) const
	{
		using namespace Engine::Serialisation;

		_j["serializer_version"] = 2;

		//// nlohmann::json does not have a way to
		//// serialize bitsets directly. Must transform it
		//// to a vector of bools first.
		//std::vector<bool> transformed_bitset(m_entity_in_use_flag.size(), false);
		//for (size_t i = 0; i < m_entity_in_use_flag.size(); i++)
		//	transformed_bitset[i] = m_entity_in_use_flag[i];

		{
			std::vector<bool> decompressed_use_flags(m_entity_in_use_flag.size(), false);
			for (size_t i = 0; i < m_entity_in_use_flag.size(); i++)
				decompressed_use_flags[i] = m_entity_in_use_flag[i];
			_j["m_entity_in_use_flag"] = compress_data_vector(decompressed_use_flags);
		}
		{
			std::vector<decltype(m_entity_counters)::value_type> decompressed_counters(m_entity_counters.size(), 0);
			for (size_t i = 0; i < m_entity_counters.size(); i++)
				decompressed_counters[i] = m_entity_counters[i];
			_j["m_entity_counters"] = compress_data_vector(decompressed_counters);
		}

		_j["m_entity_id_iter"] = m_entity_id_iter;
	}

	/*
	* Reset manager to base state. ASSUMES ALL PREVIOUS ENTITY HANDLES ARE NOT BEING USED.
	* @detail	Registered component managers are maintained however.
	*/
	void EntityManager::Reset()
	{
		for (unsigned int i = 0; i < MAX_ENTITIES; ++i)
		{
			Entity new_entity;
			new_entity.m_id = i;
			m_entity_deletion_queue.insert(new_entity);
		}
		FreeQueuedEntities();

		m_entity_in_use_flag.reset();
		memset(&m_entity_counters.front(), 0, sizeof(m_entity_counters));
		m_entity_deletion_queue.clear();
		m_entity_id_iter = 0;
	}

	/*
	* Create a single entity handle
	* @return	Entity		ID is invalid if handle could not be created.
	*/
	Entity EntityManager::EntityCreationRequest()
	{
		Entity _out_handle;
		if (!EntityCreationRequest(&_out_handle, 1))
			_out_handle.m_id = Entity::INVALID_ID;
		return _out_handle;
	}

	/*
	* Create multiple entity handles
	* @param	Entity *		Array that created handles will be output to.
	* @param	unsigned int		Number of handles that user wishes to create
	* @return	bool				False if we could not create all handles.
	* @detail	If the function returns false, the input Entity array is not modified.
	*/
	bool EntityManager::EntityCreationRequest(Entity* _out_handles, unsigned int _request_count)
	{
		if (_request_count == 0)
			return true;

		assert(_out_handles);

		unsigned int* temp_handle_idx_arr = (unsigned int*)_malloca(sizeof(unsigned int) * _request_count);
		unsigned int handles_found = 0;

		unsigned int i = m_entity_id_iter;
		do
		{
			if (!m_entity_in_use_flag.test(i))
			{
				temp_handle_idx_arr[handles_found] = i;
				handles_found++;
			}
			i = (++i % MAX_ENTITIES);
		} while (handles_found != _request_count && i != m_entity_id_iter);
		m_entity_id_iter = i + 1;

		bool found_requested_handles = (handles_found == _request_count);
		if (found_requested_handles)
		{
			for (unsigned int i = 0; i < _request_count; i++)
			{
				Entity new_handle;
				new_handle.m_id = temp_handle_idx_arr[i];
				new_handle.m_counter = m_entity_counters[temp_handle_idx_arr[i]];
				m_entity_in_use_flag.set(temp_handle_idx_arr[i], true);
				_out_handles[i] = new_handle;
			}
		}

		_freea(temp_handle_idx_arr);
		return found_requested_handles;
	}

	/*
	* Request input entity to be deleted at end of frame.
	* @param	Entity		Entity to delete
	*/
	void EntityManager::EntityDelayedDeletion(Entity _entity)
	{
		EntityDelayedDeletion(&_entity, 1);
	}

	/*
	* Request input entities to be deleted at end of frame.
	* @param	Entity *		Array of entities to delete
	* @param	unsigned int		Size of array
	*/
	void EntityManager::EntityDelayedDeletion(Entity* _entities, unsigned int _entity_count)
	{
		assert(_entities);
		std::vector<Entity> existing_entities;
		existing_entities.reserve(_entity_count);
		for (unsigned int i = 0; i < _entity_count; ++i)
		{
			// Check if entity exists before adding it to deletion queue.
			if(DoesEntityExist(_entities[i]))
				existing_entities.push_back(_entities[i]);
		}
		// Add into delayed deletion container.
		// (Containers makes sure only unique handles are added).
		m_entity_deletion_queue.insert(existing_entities.begin(), existing_entities.end());
	}

	/*
	* Checks if given entity is still "alive" (i.e. in use)
	* @param	Entity		Entity to check
	* @return	bool				True if entity is still in use
	*/
	bool EntityManager::DoesEntityExist(Entity _entity) const
	{
		return m_entity_in_use_flag.test(_entity.m_id) && (m_entity_deletion_queue.find(_entity) == m_entity_deletion_queue.end());
	}

	/*
	* Free entities queued for destruction internally
	* @return	std::vector<Entity> const 	List of entities that should be passed to all component systems
	*												in order to delete components belonging to these entities.
	*/
	std::vector<Entity> const EntityManager::FreeQueuedEntities()
	{
		// Store entities queued for deletion in list
		std::vector<Entity> deleted_entities;
		if (!m_entity_deletion_queue.empty())
		{

			deleted_entities.reserve(m_entity_deletion_queue.size());
			deleted_entities.insert(deleted_entities.begin(), m_entity_deletion_queue.begin(), m_entity_deletion_queue.end());

			// Pass destroyed entities as message to registered component managers
			for (auto component_manager : m_registered_component_managers)
				component_manager->receive_entity_destruction_message(deleted_entities);

			// Delete internal list of entities queued for destruction, and mark respective entities as unused.
			m_entity_deletion_queue.clear();
			for (unsigned int i = 0; i < deleted_entities.size(); ++i)
			{
				m_entity_counters[deleted_entities[i].ID()]++;
				m_entity_in_use_flag.reset(deleted_entities[i].ID());
			}

		}
		return deleted_entities;
	}

	void EntityManager::RegisterComponentManager(ICompManager* _component_manager)
	{
		m_registered_component_managers.insert(_component_manager);
	}

	
	void Entity::DestroyEndOfFrame()
	{
		Singleton<EntityManager>().EntityDelayedDeletion(*this);
	}

	bool Entity::Alive() const
	{
		return Singleton<EntityManager>().DoesEntityExist(*this);
	}

	const char* Entity::GetName() const
	{
		auto name_comp = GetComponent<Component::Nameable>();
		return name_comp.IsValid() ? name_comp.GetName() : "Unnamed";
	}

	void Entity::SetName(const char* _name)
	{
		auto nameable_comp = GetComponent<Component::Nameable>();
		if (!nameable_comp.IsValid())
			nameable_comp = Component::Create<Component::Nameable>(*this);
		nameable_comp.SetName(_name);
	}

	void from_json(nlohmann::json const& j, Entity& t)
	{
		t.m_data = j.get<uint16_t>();
	}

	void to_json(nlohmann::json & j, Entity const & t)
	{
		j = t.m_data;
	}

}
}