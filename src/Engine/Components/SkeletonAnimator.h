#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/manager.h>
#include <stack>
#include <tuple>

namespace Component
{
	struct Transform;

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

		void set_anim_speed(float _speed);
	};

	// Defines per-joint blending parameter
	struct animation_blend_mask
	{
		std::vector<float> m_joint_blend_masks;
		float operator[](unsigned int _index) const {
			if (m_joint_blend_masks.empty()) return 1.0f;
			else return m_joint_blend_masks[_index];
		}
	};

	struct animation_pose
	{
		animation_pose(unsigned int _skeleton_joint_count);
		std::vector<Engine::Math::transform3D> m_joint_transforms;

		void apply_blend_mask(animation_blend_mask const & _mask);
		animation_pose & apply_blend_factor(float _factor);
		animation_pose& mix(animation_pose const& _left, animation_pose const& _right, float _blend_param);
		animation_pose& mix(animation_pose const _poses[3], float const _blend_params[3]);
	};

	struct animation_tree_node;

	struct gui_node_context
	{
		unsigned int node_index = 0;
		animation_tree_node** selected_node_ptr;
	};

	struct animation_tree_node
	{
		static std::unique_ptr<animation_tree_node> create(nlohmann::json const& _j);


		virtual void compute_pose(float _time, animation_pose * _out_pose) const = 0;
		virtual float duration() const = 0;

		int gui_node(gui_node_context& _context);

		virtual void gui_edit() = 0;

		nlohmann::json serialize() const;
		void deserialize(nlohmann::json const& _json);

		std::string m_name;

	protected:

		virtual int impl_gui_node(gui_node_context & _context) = 0;
		virtual nlohmann::json impl_serialize() const = 0;
		virtual void impl_deserialize(nlohmann::json const& _json) = 0;
		virtual const char* default_name() const = 0;


		animation_blend_mask	m_blend_mask{};
	};

	struct animation_leaf_node final : public animation_tree_node
	{
		animation_handle		m_animation = 0;

		animation_leaf_node() { m_name = default_name(); }

		void set_animation(animation_handle _animation, float* _blend_mask_arr = nullptr);

		// Inherited via animation_tree_node
		virtual void compute_pose(float _time, animation_pose* _out_pose) const override;
		virtual float duration() const override;
		virtual void gui_edit() override;

	private:

		virtual int impl_gui_node(gui_node_context& _context) override;
		virtual nlohmann::json impl_serialize() const override;
		virtual void impl_deserialize(nlohmann::json const& _json) override;

		virtual const char* default_name() const override { return "Leaf Blend Node"; }
	};

	struct animation_blend_1D final : public animation_tree_node
	{
		std::vector<std::unique_ptr<animation_tree_node>> m_child_blend_nodes;
		std::vector<float> m_blendspace_points;
		std::vector<float> m_time_warps;

		float m_blend_parameter;

		animation_blend_1D() { m_name = default_name(); }

		void add_node(std::unique_ptr<animation_tree_node> && _node, float _range_start, animation_blend_mask _blend_mask);
		void edit_node_blendspace_point(unsigned int _index, float _point);
		void remove_node(unsigned int _index);

		float node_warped_duration(unsigned int _index) const;

		// Inherited via animation_tree_node
		virtual void compute_pose(float _time, animation_pose* _out_pose) const override;
		virtual float duration() const override;
		virtual void gui_edit() override;

	private:

		virtual int impl_gui_node(gui_node_context& _context) override;
		virtual nlohmann::json impl_serialize() const override;
		virtual void impl_deserialize(nlohmann::json const& _json) override;

		virtual const char* default_name() const override { return "1D Blend Node"; }

	};

	struct animation_blend_2D final : public animation_tree_node
	{
		glm::vec2 m_blend_parameter; // Barycentric coordinates in 2D space.

		std::vector<std::unique_ptr<animation_tree_node>> m_child_blend_nodes;
		std::vector<glm::vec2> m_blendspace_points;
		std::vector<float> m_time_warps;

	private:

		typedef std::tuple<uint8_t, uint8_t, uint8_t> triangle;
		typedef std::pair<triangle, glm::vec2> find_tri_result;

		// Each trinagle consists of 3 node indices, going counter-clockwise.
		std::vector<triangle> m_triangles;

		void triangulate_blendspace_points();

		find_tri_result find_triangle(glm::vec2 _blend_param) const;

	public:

		animation_blend_2D() { m_name = default_name(); }

		void add_node(std::unique_ptr<animation_tree_node>&& _node, glm::vec2 _new_point, animation_blend_mask _blend_mask);
		void edit_node_blendspace_point(unsigned int _index, glm::vec2 _new_point);
		void remove_node(unsigned int _index);

		float node_warped_duration(unsigned int _index) const;

		// Inherited via animation_tree_node
		virtual void compute_pose(float _time, animation_pose* _out_pose) const override;
		virtual float duration() const override;
		virtual void gui_edit() override;

	private:

		virtual int impl_gui_node(gui_node_context& _context) override;
		virtual nlohmann::json impl_serialize() const override;
		virtual void impl_deserialize(nlohmann::json const& _json) override;

		virtual const char* default_name() const override { return "2D Blend Node"; }
	};

	namespace AnimationUtil
	{

		float rollover_modulus(float _value, float _maximum);

		float fgcd(float l, float r);

		float find_array_fgcd(float const* _values, unsigned int _count);
		float find_array_lcm(float const* _values, unsigned int _count);

		void convert_joint_channels_to_transforms(
			animation_data const * _animation_data,
			float const* _in_anim_channel_data,
			Engine::Math::transform3D* const _joint_transforms,
			unsigned int _joint_transform_count
		);
		void compute_animation_channel_data( 
			animation_data const * _animation_data,
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

		bool				IsPaused() const;
		void				SetPaused(bool _paused);

		void SetAnimation(animation_leaf_node _animation);
		void LoadAnimation(std::string _filename);
	};

	class SkeletonAnimatorManager : public TCompManager<SkeletonAnimator>
	{
		friend struct SkeletonAnimator;

		struct animator_data
		{
			animation_instance							m_instance;
			std::unique_ptr<animation_tree_node> m_blendtree_root_node;
		};

		// TODO: Existential processing, split instances by whether they should be animated or not.
		// I.e. sort by playing / paused / invalid.
		std::unordered_map<Entity, animator_data, Entity::hash> m_entity_animator_data;


		bool m_use_slerp = true;
		Entity m_edit_tree = Entity::InvalidEntity;
		animation_tree_node * m_editing_tree_node;
		bool m_blendtree_editor_open = false;

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

		animator_data& get_entity_animator(Entity _e);

		void update_joint_transform_components(Component::Transform const* _components, Engine::Math::transform3D const* _transforms, unsigned int _joint_count);

		void window_edit_blendtree(std::unique_ptr<animation_tree_node> & _tree_root);

	public:

		virtual const char* GetComponentTypeName() const override;
		void UpdateAnimatorInstances(float _dt);

	};

	void to_json(nlohmann::json& _j, animation_blend_mask const& _mask);
	void from_json(nlohmann::json const& _j, animation_blend_mask& _mask);
}