#ifndef ENGINE_SERIALISATION_SCENE_H
#define ENGINE_SERIALISATION_SCENE_H

#include <Engine/ECS/entity.h>
#include <Engine/Serialisation/common.h>

namespace Engine {
namespace Serialisation {

	/*
	* This is passed to component managers for deserialization.
	* Useful when deserializing components that depend on other objects
	* (i.e. refers to another entity by local index)
	*/
	struct SceneContext
	{
		// Local scene entity index to in-game entity handle.
		std::vector<Engine::ECS::Entity> m_index_entities;
	};

	void DeserialiseScene(nlohmann::json const& _j);
	void SerialiseScene(nlohmann::json & _j);

}
}
#endif // !ENGINE_SERIALISATION_SCENE_H
