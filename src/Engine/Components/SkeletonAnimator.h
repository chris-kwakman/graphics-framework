#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/manager.h>

namespace Component
{
	using namespace Engine::ECS;
	using namespace Engine::Graphics;

	struct animation_instance
	{
		union
		{
			float mutable			m_anim_speed = 1.0f;
			struct
			{
				uint32_t			m_anim_speed_bits : 30;
				bool				m_loop : 1;
				bool				m_paused : 1;
			};
		};
		float						m_global_time;
		animation_handle mutable	m_animation_handle;

		void set_animation(animation_handle _anim);
		void set_anim_speed(float _speed);
	};

	namespace AnimationUtil
	{

		void update_joint_transforms(
			animation_data* const _animation_data,
			float const* _in_anim_channel_data,
			Engine::Math::transform3D* const _joint_transforms,
			unsigned int _joint_transform_count
		);
		void compute_animation_channel_data( 
			animation_data* const _animation_data,
			float _time, 
			float* _out_anim_channel_data,
			bool _use_slerp
		);
		void interpolate_vector(
			glm::vec3* _dest,
			glm::vec3 const* _left, glm::vec3 const* _right, float _right_weight,
			animation_sampler_data::E_interpolation_type _interpolation_type
		);
		void interpolate_quaternion(
			glm::quat* _dest,
			glm::quat const* _left, glm::quat const* _right, float _right_weight,
			animation_sampler_data::E_interpolation_type _interpolation_type,
			bool _use_slerp = false
		);
	}

	class SkeletonAnimatorManager;
	struct SkeletonAnimator : public IComp<SkeletonAnimatorManager>
	{
		DECLARE_COMPONENT(SkeletonAnimator);

		animation_handle	GetAnimationHandle() const;
		void				SetAnimationHandle(animation_handle _animation);
		void				SetAnimation(std::string _animationName);

		bool				IsPaused() const;
		void				SetPaused(bool _paused);
	};

	class SkeletonAnimatorManager : public TCompManager<SkeletonAnimator>
	{
		friend struct SkeletonAnimator;

		// TODO: Existential processing, split instances by whether they should be animated or not.
		// I.e. sort by playing / paused / invalid.
		std::unordered_map<Entity, animation_instance, Entity::hash> m_entity_anim_inst_map;

		bool m_use_slerp = true;

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		animation_instance& get_entity_instance(Entity _e);
		void set_instance_animation(Entity _e, animation_handle _animation);

	public:

		virtual const char* GetComponentTypeName() const override;
		void UpdateAnimatorInstances(float _dt);

	};
}