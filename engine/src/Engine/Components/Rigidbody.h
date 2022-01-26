#pragma once

#include <Engine/ECS/component_manager.h>

namespace Component
{

	using namespace Engine::ECS;

	class RigidBodyManager;
	struct RigidBody : public IComp<RigidBodyManager>
	{
		DECLARE_COMPONENT(RigidBody);
	};

	class RigidBodyManager : public TCompManager<RigidBody>
	{

	public:

		struct rigidbody_data_collection
		{
			size_t push_element(
				Entity _entity,
				glm::vec3 _position,
				glm::quat _rotation,
				float _mass,
				glm::mat3 _inertial_tensor
			);

			void erase(Entity _entity);
			bool has(Entity _entity) const;
			size_t get_entity_index(Entity _entity) const;
			bool swap_back(size_t _idx);
			bool swap_indices(size_t _idx1, size_t _idx2);
			void pop_back();
			size_t size() const { return m_positions.size(); }

			bool is_linear_integration_enabled(size_t const _entity_index) const;
			void enable_linear_integration(Entity const _entity, bool _state);

			std::unordered_map<Entity, size_t, Entity::hash> m_entity_map;
			// Index map
			std::vector<Entity> m_index_entities;

			size_t m_skip_linear_integration_count = 0;

			// ### State
			// Linear
			std::vector<glm::vec3>	m_positions;
			std::vector<glm::vec3>	m_velocities;
			std::vector<glm::vec3>	m_forces;
			// Angular
			std::vector<glm::quat>	m_rotations;
			std::vector<glm::vec3>	m_angular_moments;
			std::vector<glm::vec3>	m_torques;

			// ### Constant Properties
			std::vector<float>		m_inv_masses;
			std::vector<glm::mat3>	m_inertial_tensors;
			std::vector<glm::mat3>	m_inv_intertial_tensors;
		};

		rigidbody_data_collection m_rigidbodies_data;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void Integrate(float _dt);
		void UpdateTransforms();

		void EnableLinearIntegration(Entity const _entity, bool _state);

		void ApplyForce(Entity _entity, glm::vec3 _force, glm::vec3 _offset);
		void ApplyForce(size_t _entity_index, glm::vec3 _force, glm::vec3 _offset);

	private:

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;
	};
}