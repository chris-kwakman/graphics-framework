#include "component_manager.h"
#include <typeinfo>
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

	template<class TCompManager>
	inline typename TCompManager::comp_type IComp<TCompManager>::Create(entity_handle _e)
	{
		return get_manager().Create(_e);
	}

	///////////////////////////////////////////////////////////////
	//						ICompManager<TComp>
	///////////////////////////////////////////////////////////////


	template<class TComp>
	inline void ICompManager<TComp>::Clear()
	{
		impl_clear();
	}

	template<class TComp>
	inline typename ICompManager<TComp>::comp_type ICompManager<TComp>::Create(entity_handle _entity)
	{
		return TComp( impl_create(_entity) ? _entity : entity_handle() );
	}

	template<class TComp>
	inline void ICompManager<TComp>::Destroy(entity_handle const* _entities, unsigned int _count)
	{
		impl_destroy(_entities, _count);
	}

	template<class TComp>
	inline bool ICompManager<TComp>::ComponentOwnedByEntity(entity_handle _entity) const
	{
		return impl_component_owned_by_entity(_entity);
	}

	template<class TComp>
	inline TComp ICompManager<TComp>::Get(entity_handle _entity) const
	{
		return TComp(ComponentOwnedByEntity(_entity) ? _entity : entity_handle());
	}

}
}