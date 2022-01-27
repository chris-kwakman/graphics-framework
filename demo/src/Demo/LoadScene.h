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

		struct manager_data
		{
			std::unordered_map<std::string, uint8_t > m_scene_filepath_id;
			std::unordered_map<Entity, uint8_t, Entity::hash> m_entity_scene_id;
			std::unordered_map<uint8_t, std::unordered_set<Entity, Entity::hash>> m_scene_id_entities;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_scene_filepath_id, m_entity_scene_id, m_scene_id_entities)
		};

		manager_data m_data;

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

		void destroy_scene_id_entities(uint8_t _id);
		void set_entity_scene_id(Entity _e, uint8_t _id);

		friend struct SceneEntityComponent;

	public:
		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void DestroySceneEntities(std::string const& _scene_path);
		void DestroyAllSceneEntities();
		void RegisterScene(std::string const& _scene_path);

		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};

}