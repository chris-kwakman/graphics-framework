#include "scene.h"

#include <Engine/ECS/component_manager.h>

using namespace nlohmann;
namespace Engine {
	using namespace ECS;
namespace Serialisation {

	/*
	* Creates new entities according to given json and deserialises their corresponding component data.
	*/
	void DeserialiseScene(nlohmann::json const& _j)
	{
		//auto component_managers = ICompManager::GetRegisteredComponentManagers();
		//json const & entities = _j["Entities"];

		//// Create all entities ahead of time and store them in scene scene_context structure
		//// This scene scene_context will be passed when deserializing an entity with dependencies on other entities.
		//SceneContext scene_context;
		//scene_context.m_index_entities.resize(entities.size());
		//bool entity_creation_result = Singleton<EntityManager>().EntityCreationRequest(
		//	&scene_context.m_index_entities[0], (unsigned int)scene_context.m_index_entities.size()
		//);
		//assert(entity_creation_result);

		//// First pass, create all entity components first.
		//for (unsigned int entity_idx = 0; entity_idx < entities.size(); ++entity_idx)
		//{
		//	Entity current_entity = scene_context.m_index_entities[entity_idx];
		//	current_entity.SetName("Model Scene Node");
		//	for (auto manager : component_managers)
		//	{
		//		json const& entity_components = entities[entity_idx]["Components"];
		//		auto json_comp_iter = entity_components.find(manager->GetComponentTypeName());
		//		if (json_comp_iter != entity_components.end())
		//			manager->CreateComponent(current_entity);
		//	}
		//}
		//// Second pass, deserialize each entity's component data
		//for (unsigned int entity_idx = 0; entity_idx < entities.size(); ++entity_idx)
		//{
		//	Entity current_entity = scene_context.m_index_entities[entity_idx];
		//	for (auto manager : component_managers)
		//	{
		//		json const& entity_components = entities[entity_idx]["Components"];
		//		auto json_comp_iter = entity_components.find(manager->GetComponentTypeName());
		//		if (json_comp_iter != entity_components.end())
		//			manager->DeserializeEntityComponent(current_entity, entity_components, &scene_context);
		//	}
		//}
	}

}
}