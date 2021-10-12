#include <Engine/ECS/entity.h>
#include <nlohmann/json.hpp>

namespace Sandbox
{
	using Entity = Engine::ECS::Entity;

	nlohmann::json LoadJSON(const char* _path);
	void LoadScene(nlohmann::json const & _scene);
	std::vector<Entity> LoadGLTFScene(nlohmann::json const& _scene, const char* _scene_path);
}