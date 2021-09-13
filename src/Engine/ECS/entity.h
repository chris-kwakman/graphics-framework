#ifndef ENGINE_ECS_ENTITY_H
#define ENGINE_ECS_ENTITY_H

#include <numeric>
#include <bitset>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Engine {
namespace ECS {

	template<
		unsigned int T_BYTE_SIZE_POW_2 = 1,
		unsigned int T_COUNTER_BITS = 2
	>
	struct handle_type
	{
		static unsigned int constexpr HANDLE_BITS = 8*(1 << T_BYTE_SIZE_POW_2);
		static unsigned int constexpr COUNTER_BITS = T_COUNTER_BITS;
		static unsigned int constexpr ID_BITS = HANDLE_BITS - COUNTER_BITS;
		static unsigned int constexpr INVALID_ID = (1 << ID_BITS) - 1;

		uint16_t id() const { return m_id; }

		bool operator==(handle_type _other) const { return m_data == _other.m_data; }
		bool operator!=(handle_type _other) const { return m_data != _other.m_data; }

		// Comparison operators used for sorting.
		bool operator<(handle_type _other) const { return m_id < _other.m_id; }
		bool operator>(handle_type _other) const { return m_id > _other.m_id; }

	protected:

		uint16_t counter() const { return m_counter; }

		union
		{
			uint16_t m_data = INVALID_ID;
			struct
			{
				uint16_t m_counter : COUNTER_BITS;
				uint16_t m_id : ID_BITS;
			};
		};

		static_assert((COUNTER_BITS + ID_BITS) / 8 == sizeof(m_data));
	};

	struct entity_handle : public handle_type<1, 2>
	{
		friend class EntityManager;
	};

	class EntityManager
	{
	public:

		static unsigned int constexpr MAX_ENTITIES = (1 << entity_handle::ID_BITS) - 1;

	private:

		// Stores whether an entity ID is currently in use (ID == index in bitset)
		std::bitset<MAX_ENTITIES>	m_entity_in_use_flag;
		// Stores counter of how many times an ID has been used (ID == index in array)
		uint8_t						m_entity_counters[MAX_ENTITIES];

		unsigned int				m_entity_id_iter = 0;

		std::unordered_set<entity_handle>		m_entity_deletion_queue;

	public:

		entity_handle	EntityCreationRequest();
		bool			EntityCreationRequest(entity_handle* _out_handles, unsigned int _request_count);
		void			EntityDelayedDeletion(entity_handle _entity);
		void			EntityDelayedDeletion(entity_handle* _entities, unsigned int _entity_count);
		bool			DoesEntityExist(entity_handle _entity) const;

		std::vector<entity_handle> const FreeQueuedEntities();
	};

}
}
#endif // !ENGINE_ECS_ENTITY_H
