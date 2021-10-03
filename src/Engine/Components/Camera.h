#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/camera_data.h>

namespace Component
{
	using namespace Engine::ECS;

	class CameraManager;


	struct Camera : public IComp<CameraManager>
	{
		DECLARE_COMPONENT(Camera);

		float GetVerticalFOV() const;
		float GetNearDistance() const;
		float GetFarDistance() const;
		float GetAspectRatio() const;
		Engine::Graphics::camera_data GetCameraData() const;

		void SetVerticalFOV(float _value);
		void SetNearDistance(float _value);
		void SetFarDistance(float _value);
		void SetAspectRatio(float _value);
		void SetCameraData(Engine::Graphics::camera_data _camera_data);
	};
	class CameraManager : public TCompManager<Camera>
	{
		std::unordered_map<Entity, Engine::Graphics::camera_data, Entity::hash> m_camera_data_map;

		// Inherited via TCompManager
		bool impl_create(Entity _e) final;
		void impl_destroy(Entity const* _entities, unsigned int _count) final;
		bool impl_component_owned_by_entity(Entity _entity) const final;
		void impl_edit_component(Entity _entity) final;
		void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) final;

		Engine::Graphics::camera_data& get_camera_data(Entity _e);
		Engine::Graphics::camera_data const & get_camera_data(Entity _e) const;

		friend struct Camera;

	public:

		const char* GetComponentTypeName() const final { return "Camera"; }

		// Inherited via TCompManager
		virtual void impl_clear() override;
	};
}