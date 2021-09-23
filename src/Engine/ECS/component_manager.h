#ifndef ENGINE_ECS_COMPONENT_MANAGER_H
#define ENGINE_ECS_COMPONENT_MANAGER_H

#include "entity.h"
#include <Engine/Utils/singleton.h>

namespace Engine {
	namespace ECS {

#define DECLARE_COMPONENT(TComp) using IComp::IComp;

		template<class TCompManager>
		struct IComp
		{
			typedef TCompManager comp_Manager;
			
			IComp(entity_handle _e = entity_handle()) : m_owner(_e) {}
			IComp(IComp const & _other) : m_owner(_other.m_owner) {}

			IComp& operator=(IComp const& _other) { m_owner = _other.m_owner; return *this; }
			bool operator==(IComp const& _other) { return m_owner == _other.m_owner; }

			bool IsValid() const;
			void Destroy();

			// Shorthand for creating a component type.
			// Forwards to component manager.
			static inline typename TCompManager::comp_type Create(entity_handle _e);

			friend typename TCompManager;

		protected:

			static inline TCompManager& get_manager() { return Singleton<TCompManager>(); }

			entity_handle m_owner;
		};

		template<class TComp>
		class ICompManager
		{
		public:

			typedef TComp comp_type;

			void			Clear();

			comp_type		Create(entity_handle _entity);
			void			Destroy(entity_handle const * _entities, unsigned int _count);

			bool			ComponentOwnedByEntity(entity_handle _entity) const;
			TComp			Get(entity_handle _entity) const;

			virtual const char* GetComponentName() const = 0;

		protected:

			virtual void impl_clear() = 0;

			virtual bool impl_create(entity_handle _e) = 0;
			virtual void impl_destroy(entity_handle const * _entities, unsigned int _count) = 0;
			virtual bool impl_component_owned_by_entity(entity_handle _entity) const = 0;
		};

	}
}

#include "component_manager.inl"  

#endif // !ENGINE_ECS_COMPONENT_MANAGER_H
