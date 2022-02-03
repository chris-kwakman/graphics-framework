#include "scene.h"

#include <Engine/ECS/component_manager.h>

using namespace nlohmann;
namespace Engine {
	using namespace ECS;
namespace Serialisation {

	void DeserialiseScene(nlohmann::json const& _j)
	{
		Singleton<EntityManager>().Deserialize(_j["EntityManager"]);

		if (_j.find("resources") != _j.end())
			Singleton<Engine::Managers::ResourceManager>().ImportSceneResources(_j["resources"]);

		nlohmann::json const& json_component_managers = _j["ComponentManagers"];
		auto& component_managers = ICompManager::GetRegisteredComponentManagers();
		for each (ICompManager * mgr in component_managers)
			mgr->Deserialize(json_component_managers);
	}

	void SerialiseScene(nlohmann::json& _j)
	{
		Singleton<EntityManager>().Serialize(_j["EntityManager"]);

		Singleton<Engine::Managers::ResourceManager>().ExportSceneResources(_j["resources"]);

		nlohmann::json & json_component_managers = _j["ComponentManagers"];
		auto& component_managers = ICompManager::GetRegisteredComponentManagers();
		for each (ICompManager * mgr in component_managers)
			mgr->Serialize(json_component_managers);
	}

}
}