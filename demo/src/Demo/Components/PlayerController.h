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

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(player_data, m_velocity, m_velocity_cap, m_velocity_attenuation, m_acceleration)
		};

		struct manager_data
		{
			std::unordered_map<Entity, player_data, Entity::hash> m_player_map;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_player_map)
		};

		manager_data m_data;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		

		void update_player(float _dt, Entity _e, player_data& _data);

	public:

		void Update(float _dt);
		void SetPlayerMoveBlendParam(Entity _e, float* _param);
		void SetPlayerLookBlendParam(Entity _e, glm::vec2* _param);

		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};

}

#endif // !COMPONENTS_PLAYERCONTROLLER_H
