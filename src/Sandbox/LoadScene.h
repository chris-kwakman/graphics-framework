#include <nlohmann/json.hpp>
#include <Engine/ECS/entity.h>
#include <Engine/ECS/component_manager.h>

namespace Sandbox
{
	using Entity = Engine::ECS::Entity;

	nlohmann::json LoadJSON(const char* _path);
	void LoadScene(nlohmann::json const & _scene, const char * _scene_path);
	std::vector<Entity> LoadGLTFScene(nlohmann::json const& _scene, const char* _model_path, const char* _scene_path);


}

namespace Component
{
	class SceneEntityComponentManager;
	struct SceneEntityComponent : public Engine::ECS::IComp<SceneEntityComponentManager>
	{
		DECLARE_COMPONENT(SceneEntityComponent);
		void SetToScene(std::string const & _scene_path);
	};
	class SceneEntityComponentManager : public Engine::ECS::TCompManager<SceneEntityComponent>
	{
		using Entity = Engine::ECS::Entity;

		static uint8_t const INVALID_SCENE_ID = std::numeric_limits<uint8_t>::max();

		std::unordered_map<std::string, uint8_t > m_scene_filepath_id;
		std::unordered_map<Entity, uint8_t, Entity::hash> m_entity_scene_id;
		std::unordered_map<uint8_t, std::vector<Entity>> m_scene_id_entities;

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		void destroy_scene_id_entities(uint8_t _id);
		void set_entity_scene_id(Entity _e, uint8_t _id);

		friend struct SceneEntityComponent;

	public:
		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void DestroySceneEntities(std::string const& _scene_path);
		void DestroyAllSceneEntities();
		void RegisterScene(std::string const& _scene_path);
	};

}