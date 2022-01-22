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

	struct Entity
	{
		static unsigned int const ID_BITS = 14;
		static unsigned int const COUNTER_BITS = 2;
		static unsigned int const INVALID_ID = (1 << ID_BITS) - 1;

		union
		{
			uint16_t m_data = INVALID_ID;
			static_assert(COUNTER_BITS + ID_BITS == sizeof(m_data) * 8, "Invalid bit size for entity handle.");
			struct
			{
				decltype(m_data) m_counter : COUNTER_BITS;
				decltype(m_data) m_id : ID_BITS;
			};
		};

		struct hash
		{
			std::size_t operator()(Entity _handle) const {
				return std::hash<uint16_t>()(_handle.ID());
			}
		};

		Entity() = default;
		Entity(Entity const& _other) = default;
		Entity(Entity&& _other) = default;

		Entity& operator=(Entity const& _other) = default;
		Entity& operator=(Entity&& _other) noexcept = default;

		uint16_t	ID() const { return m_id; }
		uint16_t	Counter() const { return m_counter; }
		void		DestroyEndOfFrame();
		bool		Alive() const;
		const char* GetName() const;
		void		SetName(const char* _name);

		bool operator==(Entity const & _other) const { return m_data == _other.m_data; }
		bool operator!=(Entity const & _other) const { return m_data != _other.m_data; }

		// Comparison operators used for sorting.
		bool operator<(Entity const & _other) const { return m_id < _other.m_id; }
		bool operator>(Entity const & _other) const { return m_id > _other.m_id; }

		template<typename TComp>
		TComp GetComponent() const;

		template<typename TComp>
		bool HasComponent() const;

		friend class EntityManager;

		static Entity const InvalidEntity;
	};

	class EntityManager
	{
	public:

		EntityManager() {Reset();}

		static unsigned int constexpr MAX_ENTITIES = (1 << Entity::ID_BITS) - 1;

	private:

		// Stores whether an entity ID is currently in use (ID == index in bitset)
		std::bitset<MAX_ENTITIES>	m_entity_in_use_flag;
		// Stores Counter of how many times an ID has been used (ID == index in array)
		uint8_t						m_entity_counters[MAX_ENTITIES];

		unsigned int				m_entity_id_iter = 0;

		std::unordered_set<Entity, Entity::hash>	m_entity_deletion_queue;

		std::unordered_multiset<ICompManager*, std::hash<ICompManager*>> m_registered_component_managers;

	public:

		void			Reset();
		Entity	EntityCreationRequest();
		bool			EntityCreationRequest(Entity* _out_handles, unsigned int _request_count);
		void			EntityDelayedDeletion(Entity _entity);
		void			EntityDelayedDeletion(Entity* _entities, unsigned int _entity_count);
		bool			DoesEntityExist(Entity _entity) const;

		std::vector<Entity> const FreeQueuedEntities();

		void RegisterComponentManager(ICompManager* _component_manager);
	};

}
}
#endif // !ENGINE_ECS_ENTITY_H
