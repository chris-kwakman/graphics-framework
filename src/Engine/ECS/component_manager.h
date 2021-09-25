#ifndef ENGINE_ECS_COMPONENT_MANAGER_H
#define ENGINE_ECS_COMPONENT_MANAGER_H

#include "entity.h"
#include <Engine/Utils/singleton.h>

namespace Engine {
namespace ECS {

#define DECLARE_COMPONENT(TComp) using IComp::IComp;
#define REGISTER_COMPONENT(TComp) ;

	template<class TCompManager>
	struct IComp
	{
		typedef TCompManager comp_manager;
			
		IComp(Entity _e = Entity()) : m_owner(_e) {}
		IComp(IComp const & _other) : m_owner(_other.m_owner) {}

		IComp& operator=(IComp const& _other) { m_owner = _other.m_owner; return *this; }
		bool operator==(IComp const& _other) { return m_owner == _other.m_owner; }

		bool IsValid() const;
		void Destroy();

		// Shorthand for creating a component type.
		// Forwards to component manager.
		//static typename TCompManager::comp_type Create(Entity _e);

		friend typename TCompManager;
		friend struct Entity;

	protected:

		static inline TCompManager& get_manager() { return Singleton<TCompManager>(); }

		Entity m_owner;
	};

	class ICompManager
	{
	protected:

		void register_for_entity_destruction_message();

	public:

		virtual void receive_entity_destruction_message(std::vector<Entity> const& _destroyed_entities) = 0;

	private:

		bool m_registered_for_entity_destruction_message = false;
	};

	template<class TComp>
	class TCompManager : protected ICompManager
	{
	public:

		typedef TComp comp_type;

		void			Clear();

		comp_type		Create(Entity _entity);
		void			Destroy(Entity const * _entities, unsigned int _count);

		bool			ComponentOwnedByEntity(Entity _entity) const;
		TComp			Get(Entity _entity) const;

		void			EditComponent(Entity _entity);

		virtual const char* GetComponentName() const = 0;

	protected:

		virtual void impl_clear() = 0;

		virtual bool impl_create(Entity _e) = 0;
		virtual void impl_destroy(Entity const * _entities, unsigned int _count) = 0;
		virtual bool impl_component_owned_by_entity(Entity _entity) const = 0;
		virtual void impl_edit_component(Entity _entity) = 0;


	private:

		void receive_entity_destruction_message(std::vector<Entity> const& _destroyed_entities) final;
	};

}
}

#include "component_manager.inl"  

#endif // !ENGINE_ECS_COMPONENT_MANAGER_H
