#ifndef COMPONENTS_PLAYERCONTROLLER_H
#define COMPONENTS_PLAYERCONTROLLER_H

#include <Engine/ECS/component_manager.h>

namespace Component
{
	using namespace Engine::ECS;

	class PlayerControllerManager;
	struct PlayerController : IComp<PlayerControllerManager>
	{
		void BindMoveBlendParameter(float* _param);
		void BindLookBlendParameter(glm::vec2* _param);

		DECLARE_COMPONENT(PlayerController);
	};
	class PlayerControllerManager : public TCompManager<PlayerController>
	{
		struct player_data
		{
			glm::vec2 m_velocity;
			float m_velocity_cap;
			float m_velocity_attenuation = 0.9f;
			float m_acceleration = 20.0f;
			float* m_anim_ptr_move_blend = nullptr;
			glm::vec2* m_anim_ptr_look_blend = nullptr;
		};

		std::unordered_map<Entity, player_data, Entity::hash> m_player_map;


		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		void update_player(float _dt, Entity _e, player_data& _data);

	public:

		void Update(float _dt);
		void SetPlayerMoveBlendParam(Entity _e, float* _param);
		void SetPlayerLookBlendParam(Entity _e, glm::vec2* _param);
	};

}

#endif // !COMPONENTS_PLAYERCONTROLLER_H
