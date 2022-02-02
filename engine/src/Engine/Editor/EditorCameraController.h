#include <engine/ECS/component_manager.h>
#include <engine/Graphics/camera_data.h>

namespace Component
{
	using namespace Engine::ECS;

	class EditorCameraControllerManager;
	struct EditorCameraController : public IComp<EditorCameraControllerManager>
	{
		DECLARE_COMPONENT(EditorCameraController);

		void SetAsActiveEditorCamera();
	};

	class EditorCameraControllerManager : public TCompManager<EditorCameraController>
	{
		friend struct EditorCameraController;

		struct manager_data
		{
			struct controller_properties
			{
				float speed = 10.0f;
				float fast_speed = 20.0f;
				float rotate_speed = 3.1415f * 0.75f;
				float mouse_sensitivity = 0.05f;
				bool inverted = false;

				NLOHMANN_DEFINE_TYPE_INTRUSIVE(controller_properties, speed, fast_speed, rotate_speed, mouse_sensitivity, inverted);
			};

			std::unordered_map<Entity, controller_properties, Entity::hash> m_registered_entities;
			Entity m_active_camera = Entity::InvalidEntity;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_registered_entities, m_active_camera);
		};

		manager_data m_data;

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;

	public:

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void UpdateActiveEditorCamera(float const _dt);

	};
}