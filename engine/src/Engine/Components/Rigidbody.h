#pragma once

#include <Engine/ECS/component_manager.h>
#include <Engine/Physics/rigidbody_data.hpp>

namespace Component
{

	using namespace Engine::ECS;

	class RigidBodyManager;
	struct RigidBody : public IComp<RigidBodyManager>
	{
		DECLARE_COMPONENT(RigidBody);

		Engine::Physics::rigidbody_data	GetRigidBodyData() const;
		void SetRigidBodyData(Engine::Physics::rigidbody_data const& _rb_data);
		void UseColliderInertia();
	};

	class RigidBodyManager : public TCompManager<RigidBody>
	{

		friend RigidBody;

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
			std::vector<glm::vec3>	m_linear_momentums;
			std::vector<glm::vec3>	m_force_accumulators;
			// Angular
			std::vector<glm::quat>	m_rotations;
			std::vector<glm::vec3>	m_angular_momentums;
			std::vector<glm::vec3>	m_torque_accumulators;

			// ### Constant Properties
			std::vector<float>		m_inv_masses;
			std::vector<glm::mat3>	m_inertial_tensors;
			std::vector<glm::mat3>	m_inv_inertial_tensors;
			std::vector<float>		m_restitution;
			std::vector<float>		m_friction_coefficient; // [0,1]

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(
				rigidbody_data_collection,
				m_entity_map,
				m_index_entities,
				m_skip_linear_integration_count,
				m_positions,
				m_linear_momentums,
				//m_force_accumulators,		// Should always be zero at end of each frame
				m_rotations,
				m_angular_momentums,
				//m_torque_accumulators,	// Should always be zero at end of each frame
				m_inv_masses,
				m_inertial_tensors,
				m_restitution,
				m_friction_coefficient
				//m_inv_inertial_tensors // No need to serialize / deserialize, we can derive it from original tensor.
			)
		};

		rigidbody_data_collection m_rigidbodies_data;
		bool m_integration_enabled = true;
		bool m_positions_updated = true;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

		void Integrate(float _dt);
		void ClearForces();
		void UpdateTransforms();

		void EnableLinearIntegration(Entity const _entity, bool _state);

		void ApplyForce(Entity _entity, glm::vec3 _force, glm::vec3 _offset);
		void ApplyForce(size_t _entity_index, glm::vec3 _force, glm::vec3 _offset);
		void SetInertialTensor(size_t _entity_index, glm::mat3 _inertial_tensor);

		Engine::Physics::rigidbody_data	GetEntityRigidBodyData(Entity _e) const;
		void							SetEntityRigidBodyData(Entity _e, Engine::Physics::rigidbody_data const & _rb_data);

	private:

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}