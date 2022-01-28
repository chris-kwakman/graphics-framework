#include "scene.h"

#include <Engine/ECS/component_manager.h>

using namespace nlohmann;
namespace Engine {
	using namespace ECS;
namespace Serialisation {

	void DeserialiseScene(nlohmann::json const& _j)
	{
		Singleton<EntityManager>().Deserialize(_j["EntityManager"]);

		nlohmann::json const& json_component_managers = _j["ComponentManagers"];
		auto& component_managers = ICompManager::GetRegisteredComponentManagers();
		for each (ICompManager * mgr in component_managers)
			mgr->Deserialize(json_component_managers);
	}

	void SerialiseScene(nlohmann::json& _j)
	{
		Singleton<EntityManager>().Serialize(_j["EntityManager"]);

		nlohmann::json & json_component_managers = _j["ComponentManagers"];
		auto& component_managers = ICompManager::GetRegisteredComponentManagers();
		for each (ICompManager * mgr in component_managers)
			mgr->Serialize(json_component_managers);
	}

}
}