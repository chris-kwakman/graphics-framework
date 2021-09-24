#ifndef ENGINE_ECS_ENTITY_H
#define ENGINE_ECS_ENTITY_H

#include <numeric>
#include <bitset>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Engine {
namespace ECS {

	class ICompManager;

	struct entity_handle
	{
		static unsigned int const ID_BITS = 12;
		static unsigned int const COUNTER_BITS = 4;
		static unsigned int const INVALID_ID = (1 << ID_BITS) - 1;

		union
		{
			uint16_t m_data = INVALID_ID;
			static_assert(COUNTER_BITS + ID_BITS == sizeof(m_data) * 8);
			struct
			{
				decltype(m_data) m_counter : COUNTER_BITS;
				decltype(m_data) m_id : ID_BITS;
			};
		};

		struct hash
		{
			std::size_t operator()(entity_handle _handle) const {
				return std::hash<uint16_t>()(_handle.id());
			}
		};

		entity_handle() = default;
		entity_handle(entity_handle const& _other) = default;
		entity_handle(entity_handle&& _other) = default;

		entity_handle& operator=(entity_handle const& _other) = default;
		entity_handle& operator=(entity_handle&& _other) noexcept = default;

		uint16_t	id() const { return m_id; }
		uint16_t	counter() const { return m_counter; }
		void		delayed_destruction();

		bool operator==(entity_handle const & _other) const { return m_data == _other.m_data; }
		bool operator!=(entity_handle const & _other) const { return m_data != _other.m_data; }

		// Comparison operators used for sorting.
		bool operator<(entity_handle const & _other) const { return m_id < _other.m_id; }
		bool operator>(entity_handle const & _other) const { return m_id > _other.m_id; }

		bool is_alive() const;

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

		std::unordered_set<entity_handle, entity_handle::hash>	m_entity_deletion_queue;

		std::unordered_multiset<ICompManager*, std::hash<ICompManager*>> m_registered_component_managers;

	public:

		void			Reset();
		entity_handle	EntityCreationRequest();
		bool			EntityCreationRequest(entity_handle* _out_handles, unsigned int _request_count);
		void			EntityDelayedDeletion(entity_handle _entity);
		void			EntityDelayedDeletion(entity_handle* _entities, unsigned int _entity_count);
		bool			DoesEntityExist(entity_handle _entity) const;

		std::vector<entity_handle> const FreeQueuedEntities();

		void RegisterComponentManager(ICompManager* _component_manager);
	};

}
}
#endif // !ENGINE_ECS_ENTITY_H
