#include "component_manager.h"
#include "entity.h"

namespace Engine {
namespace ECS {

	static std::vector<ICompManager*> s_registered_component_managers;

	std::vector<ICompManager*> const& ICompManager::GetRegisteredComponentManagers()
	{
		return s_registered_component_managers;
	}

	bool ICompManager::RegisterComponentManager(ICompManager* _component_manager)
	{
		Singleton<EntityManager>().RegisterComponentManager(_component_manager);
		s_registered_component_managers.push_back(_component_manager);
		return true;
	}

}
}