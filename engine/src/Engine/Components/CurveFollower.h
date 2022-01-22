#ifndef COMPONENTS_CURVEFOLLOWER_H
#define COMPONENTS_CURVEFOLLOWER_H

#include <Engine/ECS/component_manager.h>
#include "CurveInterpolator.h"

namespace Component
{

	using namespace Engine::ECS;

	struct follower_data
	{
		enum distance_time_func : uint8_t {
			eLinear,	// Constant rate of travel
			eSineInterpolation,	// EaseIn/Out w/ modifiable linear segment
		};

		float				m_arclength = 0.0f;
		union
		{
			float			m_max_travel_rate;
			struct
			{
				uint32_t	m_distance_bits : 31;
				bool		m_loop : 1;
			};
		};
		union
		{
			struct 
			{
				float		m_travel_rate;
			} m_lin_dist_time_data;
			struct
			{
				float		m_travel_rate; // Travel rate in normalized parameter space.
				float		m_seg_front_param;	// Normalized parameter
				float		m_seg_back_param;	// Normalized parameter
			} m_sine_interp_dist_time_data;
		};
		CurveInterpolator	m_curve_component; // Drag-droppeable / settable
		distance_time_func	m_distance_time_func = distance_time_func::eLinear;

		void set_linear_dist_time_func_data(float _travel_rate);
		void set_sine_interp_dist_time_func_data(float _param_travel_rate, float _seg_front_param, float _seg_back_param);

		friend class CurveFollowerManager;
	};

	class CurveFollowerManager;
	struct CurveFollower final : public IComp<CurveFollowerManager>
	{
		DECLARE_COMPONENT(CurveFollower);

		follower_data & GetFollowerData();

		bool GetPlayingState() const;
		void SetPlayingState(bool _state);
	};
	class CurveFollowerManager final : public TCompManager<CurveFollower>
	{
		std::unordered_map<Entity, follower_data, Entity::hash> m_follower_map;

		std::unordered_set<Entity, Entity::hash> m_animateable;

		void set_follower_distance(Entity _e, follower_data& _data, float _distance);
		void set_follower_playing_state(Entity _e, bool _play_state);

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		friend struct CurveFollower;

	public:

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void UpdateFollowers(float _dt);
	};

}

#endif // !COMPONENTS_CURVEFOLLOWER_H
