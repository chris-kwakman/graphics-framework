#include "component_manager.h"
#include <typeinfo>
#include "entity.h"

#include <ImGui/imgui.h>

#include <Engine/Serialisation/scene.h>

namespace Engine {
namespace ECS {

	///////////////////////////////////////////////////////////////
	//					IComp<TCompManager>
	///////////////////////////////////////////////////////////////

	template<class TCompManager>
	inline bool IComp<TCompManager>::IsValid() const
	{
		return get_manager().ComponentOwnedByEntity(m_owner);
	}

	template<class TCompManager>
	inline void IComp<TCompManager>::Destroy()
	{
		get_manager().Destroy(&m_owner, 1);
	}

	///////////////////////////////////////////////////////////////
	//						TCompManager<TComp>
	///////////////////////////////////////////////////////////////


	template<class TComp>
	inline void TCompManager<TComp>::Clear()
	{
		impl_clear();
	}

	template<class TComp>
	inline typename TCompManager<TComp>::comp_type TCompManager<TComp>::Create(Entity _entity)
	{
		return TComp(
			(!impl_component_owned_by_entity(_entity) && impl_create(_entity))
			? _entity 
			: Entity() 
		);
	}

	template<class TComp>
	inline void TCompManager<TComp>::Destroy(Entity const* _entities, unsigned int _count)
	{
		impl_destroy(_entities, _count);
	}

	template<class TComp>
	inline bool TCompManager<TComp>::ComponentOwnedByEntity(Entity _entity) const
	{
		return impl_component_owned_by_entity(_entity);
	}

	template<class TComp>
	inline TComp TCompManager<TComp>::Get(Entity _entity) const
	{
		return TComp(ComponentOwnedByEntity(_entity) ? _entity : Entity());
	}

	template<class TComp>
	inline void TCompManager<TComp>::EditComponent(Entity _entity)
	{
		if (ComponentOwnedByEntity(_entity) && ImGui::CollapsingHeader(GetComponentTypeName()))
			impl_edit_component(_entity);
	}

	template<class TComp>
	inline void TCompManager<TComp>::DeserializeEntityComponent(Entity _e, nlohmann::json const& _json_entity, Engine::Serialisation::SceneContext const* _context)
	{
		auto component_iter = _json_entity.find(GetComponentTypeName());
		if (component_iter != _json_entity.end())
			impl_deserialise_component(_e, *component_iter, _context);
	}

	template<class TComp>
	inline void TCompManager<TComp>::receive_entity_destruction_message(std::vector<Entity> const& _destroyed_entities)
	{
		Destroy(&_destroyed_entities.front(), (unsigned int)_destroyed_entities.size());
	}

	template<typename TComp>
	inline TComp Entity::GetComponent() const
	{
		static_assert(std::is_base_of<IComp<typename TComp::comp_manager>,TComp>::value);
#ifdef _DEBUG
		if (HasComponent<TComp>())
			return TComp::get_manager().Get(*this);
		else
			return TComp();
#else
		return TComp::get_manager().Get(*this);
#endif // _DEBUG
	}

	template<typename TComp>
	inline bool Entity::HasComponent() const
	{
		return TComp::get_manager().ComponentOwnedByEntity(*this);
	}
}
}