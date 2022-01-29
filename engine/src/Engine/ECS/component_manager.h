#ifndef ENGINE_ECS_COMPONENT_MANAGER_H
#define ENGINE_ECS_COMPONENT_MANAGER_H

#include <Engine/Serialisation/common.h>
#include <Engine/Utils/singleton.h>
#include "entity.h"
#include <vector>

namespace Engine {
namespace ECS {

#define DECLARE_COMPONENT(TComp) using IComp::IComp; bool operator==(TComp const & _o) const {return Owner() == _o.Owner();} bool operator==(Engine::ECS::Entity _e) const {return Owner() == _e;}

	template<class TCompManager>
	struct IComp
	{
		typedef TCompManager comp_manager;
			
		IComp(Entity _e = Entity()) : m_owner(_e) {}
		IComp(IComp const & _other) : m_owner(_other.m_owner) {}

		IComp& operator=(IComp const& _other) { m_owner = _other.m_owner; return *this; }
		bool operator==(IComp const& _other) { return m_owner == _other.m_owner; }

		Entity Owner() const;

		bool IsValid() const;
		void Destroy();

		friend typename TCompManager;
		friend struct Entity;

		static inline TCompManager& GetManager() { return Singleton<TCompManager>(); }

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(IComp, m_owner)

	protected:

		Entity m_owner;
	};

	class ICompManager
	{

	public:

		virtual void CreateComponent(Entity _entity) = 0;
		virtual void DestroyComponent(Entity _entity) = 0;

		virtual void EditComponent(Entity _entity) = 0;
		virtual const char* GetComponentTypeName() const = 0;

		virtual void	Deserialize(nlohmann::json const& _j) = 0;
		virtual void	Serialize(nlohmann::json& _j) const = 0;

		static std::vector<ICompManager*> const& GetRegisteredComponentManagers();

	protected:

		static bool RegisterComponentManager(ICompManager* _component_manager);

	private:

		virtual void receive_entity_destruction_message(std::vector<Entity> const& _destroyed_entities) = 0;
		bool mb_registered = false;

		friend class Engine::ECS::EntityManager;
	};

	template<class TComp>
	class TCompManager : protected ICompManager
	{
	public:

		void Initialize() { RegisterComponentManager(this); }

		typedef TComp comp_type;

		void			Clear();

		comp_type		Create(Entity _entity);
		void			Destroy(Entity const * _entities, unsigned int _count);

		bool			ComponentOwnedByEntity(Entity _entity) const;
		TComp			Get(Entity _entity) const;

		void			EditComponent(Entity _entity) final;

		virtual const char* GetComponentTypeName() const = 0;
	protected:

		void			CreateComponent(Entity _entity) final { Create(_entity); }
		void			DestroyComponent(Entity _entity) final { Destroy(&_entity, 1); }

		void	Deserialize(nlohmann::json const& _j) final;
		void	Serialize(nlohmann::json& _j) const final;

		virtual void impl_clear() = 0;

		virtual bool impl_create(Entity _e) = 0;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) = 0;
		virtual bool impl_component_owned_by_entity(Entity _entity) const = 0;
		virtual void impl_edit_component(Entity _entity) = 0;
		
		virtual void impl_deserialize_data(nlohmann::json const& _j) = 0;
		virtual void impl_serialize_data(nlohmann::json& _j) const = 0;


	private:

		//static bool mb_registered;
		//static bool static_initializer();

		void receive_entity_destruction_message(std::vector<Entity> const& _destroyed_entities) final;
	};

}
}

namespace Component
{
	template<typename TComp>
	TComp Create(Engine::ECS::Entity _e) {
		return TComp::GetManager().Create(_e);
	}

}

#include "component_manager.inl"  

#endif // !ENGINE_ECS_COMPONENT_MANAGER_H
