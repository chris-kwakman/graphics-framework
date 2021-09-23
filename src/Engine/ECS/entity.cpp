#include "entity.h"
#include <cassert>

#include <Engine/Utils/singleton.h>

namespace Engine {
namespace ECS {

	/*
	* Create a single entity handle
	* @return	entity_handle		ID is invalid if handle could not be created.
	*/
	entity_handle EntityManager::EntityCreationRequest()
	{
		entity_handle _out_handle;
		if (!EntityCreationRequest(&_out_handle, 1))
			_out_handle.m_id = entity_handle::INVALID_ID;
		return _out_handle;
	}

	/*
	* Create multiple entity handles
	* @param	entity_handle *		Array that created handles will be output to.
	* @param	unsigned int		Number of handles that user wishes to create
	* @return	bool				False if we could not create all handles.
	* @detail	If the function returns false, the input entity_handle array is not modified.
	*/
	bool EntityManager::EntityCreationRequest(entity_handle* _out_handles, unsigned int _request_count)
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
				temp_handle_idx_arr[i] = i;
				handles_found++;
			}
		} while (handles_found != _request_count && (++i % MAX_ENTITIES) != m_entity_id_iter);

		bool found_requested_handles = (handles_found == _request_count);
		if (found_requested_handles)
		{
			for (unsigned int i = 0; i < _request_count; i++)
			{
				entity_handle new_handle;
				new_handle.m_id = temp_handle_idx_arr[i];
				new_handle.m_counter = m_entity_counters[temp_handle_idx_arr[i]];
				_out_handles[i] = new_handle;
			}
		}

		_freea(temp_handle_idx_arr);
		return found_requested_handles;
	}

	/*
	* Request input entity to be deleted at end of frame.
	* @param	entity_handle		Entity to delete
	*/
	void EntityManager::EntityDelayedDeletion(entity_handle _entity)
	{
		EntityDelayedDeletion(&_entity, 1);
	}

	/*
	* Request input entities to be deleted at end of frame.
	* @param	entity_handle *		Array of entities to delete
	* @param	unsigned int		Size of array
	*/
	void EntityManager::EntityDelayedDeletion(entity_handle* _entities, unsigned int _entity_count)
	{
		assert(_entities);
		std::vector<entity_handle> existing_entities;
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
	* @param	entity_handle		Entity to check
	* @return	bool				True if entity is still in use
	*/
	bool EntityManager::DoesEntityExist(entity_handle _entity) const
	{
		return m_entity_in_use_flag.test(_entity.m_id);
	}

	/*
	* Free entities queued for destruction internally
	* @return	std::vector<entity_handle> const 	List of entities that should be passed to all component systems
	*												in order to delete components belonging to these entities.
	*/
	std::vector<entity_handle> const EntityManager::FreeQueuedEntities()
	{
		// Store entities queued for deletion in list
		std::vector<entity_handle> deleted_entities;
		deleted_entities.reserve(m_entity_deletion_queue.size());
		deleted_entities.insert(deleted_entities.begin(), m_entity_deletion_queue.begin(), m_entity_deletion_queue.end());

		// Delete internal list of entities queued for destruction, and mark respective entities as unused.
		m_entity_deletion_queue.clear();
		for (unsigned int i = 0; i < deleted_entities.size(); ++i)
		{
			m_entity_counters[deleted_entities[i].id()]++;
			m_entity_in_use_flag.reset(deleted_entities[i].id());
		}

		return deleted_entities;
	}

	
	bool entity_handle::is_alive() const
	{
		return Singleton<EntityManager>().DoesEntityExist(*this);
	}

}
}