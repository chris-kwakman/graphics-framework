#include "Rigidbody.h"
#include "Transform.h"

#include <ImGui/imgui.h>

namespace Component
{
	using namespace Engine::ECS;

	const char* RigidBodyManager::GetComponentTypeName() const
	{
		return "RigidBody";
	}

	void RigidBodyManager::impl_clear()
	{
		m_rigidbodies_data = rigidbody_data_collection();
		m_entity_map.clear();
	}

	bool RigidBodyManager::impl_create(Entity _e)
	{
		Transform entity_transform = _e.GetComponent<Transform>();
		glm::vec3 assign_position = glm::vec3(0.0f);
		glm::quat assign_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		if (entity_transform.IsValid())
		{
			Engine::Math::transform3D const world_transform = entity_transform.ComputeWorldTransform();
			assign_position = world_transform.position;
			assign_rotation = world_transform.quaternion;
		}
		size_t const new_element_index = m_rigidbodies_data.push_element(
			_e,
			assign_position, 
			assign_rotation, 
			1.0f, 
			glm::mat3(1.0f)
		);
		m_entity_map.emplace(_e, new_element_index);
		
		return true;
	}

	void RigidBodyManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			Entity const delete_entity = _entities[i];
			auto entity_map_iterator = m_entity_map.find(delete_entity);
			if (entity_map_iterator == m_entity_map.end())
				continue;
			
			size_t const pop_index = entity_map_iterator->second;
			
			// Update entity to index mapping.
			if(m_rigidbodies_data.swap_back(pop_index))
				m_entity_map[m_rigidbodies_data.m_index_entities[pop_index]] = pop_index;

			m_rigidbodies_data.pop_back();
		}
	}

	bool RigidBodyManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_entity_map.find(_entity) != m_entity_map.end();
	}

	void RigidBodyManager::impl_edit_component(Entity _entity)
	{
		size_t const entity_index = m_entity_map[_entity];
		ImGui::DragFloat3("Position", (float*)&m_rigidbodies_data.m_positions[entity_index], 1.0f, -FLT_MAX, FLT_MAX, "%.3f", 1.0f);
		ImGui::DragFloat3("Velocity", (float*)&m_rigidbodies_data.m_velocities[entity_index], 1.0f, -100.0f, 100.0f, "%.3f", 1.0f);

		if (ImGui::DragFloat4("Rotation", (float*)&m_rigidbodies_data.m_rotations[entity_index], 0.05f, -1.0f, 1.0f, "%.3f"))
			m_rigidbodies_data.m_rotations[entity_index] = glm::normalize(m_rigidbodies_data.m_rotations[entity_index]);
		ImGui::DragFloat3("Omega", (float*)&m_rigidbodies_data.m_omegas[entity_index]);
		float mass = m_rigidbodies_data.m_inv_masses[entity_index];
		if (mass <= 0.0f)
			mass = std::clamp(mass, 0.0f, FLT_MAX);
		else
			mass = 1.0f / mass;
		if(ImGui::DragFloat("Mass", &mass, 1.0f, 0.0f, FLT_MAX, "%.3f"))
		{
			if (mass <= 0.0f)
				m_rigidbodies_data.m_inv_masses[entity_index] = 0.0f;
			else
				m_rigidbodies_data.m_inv_masses[entity_index] = 1.0f / mass;
		}


	}

	void RigidBodyManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}



	size_t RigidBodyManager::rigidbody_data_collection::push_element(
		Entity _entity,
		glm::vec3 _position, 
		glm::quat _rotation, 
		float _mass, 
		glm::mat3 _inertial_tensor
	)
	{
		size_t const new_element_index = m_positions.size();

		m_index_entities.push_back(_entity);

		// States
		// Linear
		m_positions.push_back(_position);
		m_velocities.push_back(glm::vec3(0.0f));
		m_forces.push_back(glm::vec3(0.0f));
		// Angular
		m_rotations.push_back(_rotation);
		m_omegas.push_back(glm::vec3(0.0f));
		m_torques.push_back(glm::vec3(0.0f));

		// Properties
		m_inv_masses.push_back(_mass);
		m_inertial_tensors.push_back(_inertial_tensor);
		m_inv_intertial_tensors.push_back(glm::inverse(_inertial_tensor));

		return new_element_index;
	}

	bool RigidBodyManager::rigidbody_data_collection::swap_back(size_t _idx)
	{
		size_t const last_index = size() - 1;
		return swap_indices(_idx, last_index);
	}

	bool RigidBodyManager::rigidbody_data_collection::swap_indices(size_t _idx1, size_t _idx2)
	{
		if (_idx1 == _idx2)
			return false;

		auto swap_indices = [_idx1, _idx2](auto& _container)
		{
			std::swap(_container[_idx1], _container[_idx2]);
		};

		// Map
		swap_indices(m_index_entities);

		// Linear
		swap_indices(m_positions);
		swap_indices(m_velocities);
		swap_indices(m_forces);
		// Angular
		swap_indices(m_rotations);
		swap_indices(m_omegas);
		swap_indices(m_torques);
		// Constant properties
		swap_indices(m_inv_masses);
		swap_indices(m_inertial_tensors);
		swap_indices(m_inv_intertial_tensors);

		return true;
	}

	void RigidBodyManager::rigidbody_data_collection::pop_back()
	{
		m_index_entities.pop_back();
		m_positions.pop_back();
		m_velocities.pop_back();
		m_forces.pop_back();
		m_rotations.pop_back();
		m_omegas.pop_back();
		m_torques.pop_back();
		m_inv_masses.pop_back();
		m_inertial_tensors.pop_back();
		m_inv_intertial_tensors.pop_back();
	}

}