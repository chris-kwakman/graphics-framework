#include "component_manager.h"
#include "entity.h"

namespace Engine {
namespace ECS {

	void ICompManager::register_for_entity_destruction_message()
	{
		if (!m_registered_for_entity_destruction_message)
		{
			m_registered_for_entity_destruction_message = true;
			Singleton<EntityManager>().RegisterComponentManager(this);
		}
	}

}
}