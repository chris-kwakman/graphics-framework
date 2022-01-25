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
	inline Entity IComp<TCompManager>::Owner() const
	{
		return m_owner;
	}

	template<class TCompManager>
	inline bool IComp<TCompManager>::IsValid() const
	{
		return m_owner != Entity::InvalidEntity && GetManager().ComponentOwnedByEntity(m_owner);
	}

	template<class TCompManager>
	inline void IComp<TCompManager>::Destroy()
	{
		GetManager().Destroy(&m_owner, 1);
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
		if (ComponentOwnedByEntity(_entity))
		{
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
			bool const opened_collapsing_header = ImGui::CollapsingHeader(GetComponentTypeName());
			bool component_destroyed = false;
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::Button("Destroy Component"))
				{
					Destroy(&_entity, 1);
					component_destroyed = true;
				}
				ImGui::EndPopup();
			}
			if (opened_collapsing_header && !component_destroyed)
			{
				ImGui::PushID(GetComponentTypeName());
				impl_edit_component(_entity);
				ImGui::PopID();
			}
		}
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
			return TComp::GetManager().Get(*this);
		else
			return TComp();
#else
		return TComp::GetManager().Get(*this);
#endif // _DEBUG
	}

	template<typename TComp>
	inline bool Entity::HasComponent() const
	{
		return TComp::GetManager().ComponentOwnedByEntity(*this);
	}
}
}