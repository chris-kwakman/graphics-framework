#include "Rigidbody.h"
#include "Transform.h"

#include <Engine/Physics/integration.h>
#include <Engine/Editor/editor.h>

#include <algorithm>

namespace Component
{
	using namespace Engine::ECS;

	const char* RigidBodyManager::GetComponentTypeName() const
	{
		return "RigidBody";
	}

	void RigidBodyManager::Integrate(float _dt)
	{
		using namespace Engine::Physics;
		size_t const rigidbody_count = m_rigidbodies_data.size();

		// Skip if there's nothing to integrate.
		if (rigidbody_count == 0)
			return;

		// Read transform position & rotation from each object
		for (size_t i = 0; i < rigidbody_count; i++)
		{
			Transform entity_transform_component = m_rigidbodies_data.m_index_entities[i].GetComponent<Transform>();
			assert(entity_transform_component.GetParent() == Entity::InvalidEntity && "Rigidbody entities are assumed to have no parent.");
			m_rigidbodies_data.m_positions[i] = entity_transform_component.GetLocalPosition();
			m_rigidbodies_data.m_rotations[i] = entity_transform_component.GetLocalRotation();
		}

		integrate_linear(
			_dt,
			&m_rigidbodies_data.m_positions.front(),
			&m_rigidbodies_data.m_velocities.front(),
			&m_rigidbodies_data.m_forces.front(),
			&m_rigidbodies_data.m_inv_masses.front(),
			rigidbody_count - m_rigidbodies_data.m_skip_linear_integration_count
		);

		integrate_angular(
			_dt,
			&m_rigidbodies_data.m_rotations.front(),
			&m_rigidbodies_data.m_angular_moments.front(),
			&m_rigidbodies_data.m_torques.front(),
			&m_rigidbodies_data.m_inv_inertial_tensors.front(),
			rigidbody_count
		);

		// Reset forces
		for (size_t i = 0; i < rigidbody_count; i++)
		{
			m_rigidbodies_data.m_forces[i] = glm::vec3(0.0f);
			m_rigidbodies_data.m_torques[i] = glm::vec3(0.0f);
		}
	}

	void RigidBodyManager::UpdateTransforms()
	{
		size_t const rigidbody_count = m_rigidbodies_data.size();
		for (size_t i = 0; i < rigidbody_count; i++)
		{
			Transform entity_transform_component = m_rigidbodies_data.m_index_entities[i].GetComponent<Transform>();
			entity_transform_component.SetLocalPosition(m_rigidbodies_data.m_positions[i]);
			entity_transform_component.SetLocalRotation(m_rigidbodies_data.m_rotations[i]);
		}
	}

	void RigidBodyManager::EnableLinearIntegration(Entity const _entity, bool _state)
	{
		m_rigidbodies_data.enable_linear_integration(_entity, _state);
	}

	void RigidBodyManager::ApplyForce(Entity _entity, glm::vec3 _force, glm::vec3 _offset)
	{
		ApplyForce(m_rigidbodies_data.get_entity_index(_entity), _force, _offset);
	}

	void RigidBodyManager::ApplyForce(size_t _entity_index, glm::vec3 _force, glm::vec3 _offset)
	{
		m_rigidbodies_data.m_forces[_entity_index] += _force;
		m_rigidbodies_data.m_torques[_entity_index] += glm::cross(_offset, _force);
	}

	void RigidBodyManager::impl_clear()
	{
		m_rigidbodies_data = rigidbody_data_collection();
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
			assign_rotation = world_transform.rotation;
		}
		size_t const new_element_index = m_rigidbodies_data.push_element(
			_e,
			assign_position, 
			assign_rotation, 
			1.0f, 
			glm::mat3(1.0f)
		);
		
		return true;
	}

	void RigidBodyManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			if (m_rigidbodies_data.has(_entities[i]))
				m_rigidbodies_data.erase(_entities[i]);
		}
	}

	bool RigidBodyManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_rigidbodies_data.has(_entity);
	}

	void RigidBodyManager::impl_edit_component(Entity _entity)
	{
		size_t const entity_index = m_rigidbodies_data.get_entity_index(_entity);
		ImGui::DragFloat3("Position", (float*)&m_rigidbodies_data.m_positions[entity_index], 1.0f, -FLT_MAX, FLT_MAX, "%.3f", 1.0f);
		ImGui::DragFloat3("Velocity", (float*)&m_rigidbodies_data.m_velocities[entity_index], 1.0f, -100.0f, 100.0f, "%.3f", 1.0f);

		if (ImGui::DragFloat4("Rotation", (float*)&m_rigidbodies_data.m_rotations[entity_index], 0.05f, -1.0f, 1.0f, "%.3f"))
			m_rigidbodies_data.m_rotations[entity_index] = glm::normalize(m_rigidbodies_data.m_rotations[entity_index]);
		ImGui::DragFloat3("Angular Moment", (float*)&m_rigidbodies_data.m_angular_moments[entity_index]);
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

		bool linear_integration_active = m_rigidbodies_data.is_linear_integration_enabled(entity_index);
		if (ImGui::Checkbox("Linear Integration", &linear_integration_active))
			EnableLinearIntegration(_entity, linear_integration_active);

		auto edit_mat3 = [](glm::mat3& _edit, const char* _name)->bool
		{
			bool modified = false;
			ImGuiTableFlags const table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;
			ImGui::Text(_name);
			if (ImGui::BeginTable(_name, 3, table_flags))
			{
				for (size_t i = 0; i < 3; i++)
				{
					ImGui::TableNextRow();
					for (size_t j = 0; j < 3; j++)
					{
						ImGui::PushID(3 * j + i);
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						modified |= ImGui::InputFloat("##flt_input", &_edit[j][i], 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
						ImGui::PopID();
					}
				}
			}
			ImGui::EndTable();
			return modified;
		};

		bool edited_inertial_tensor = edit_mat3(m_rigidbodies_data.m_inertial_tensors[entity_index], "Inertial Tensor");
		if (edited_inertial_tensor)
			m_rigidbodies_data.m_inv_inertial_tensors[entity_index] = glm::inverse(m_rigidbodies_data.m_inertial_tensors[entity_index]);

		ImGui::BeginDisabled(true);
		edit_mat3(m_rigidbodies_data.m_inv_inertial_tensors[entity_index], "Inverse Inertial Tensor");
		ImGui::EndDisabled();

	}

	void RigidBodyManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_rigidbodies_data = _j["rigidbody_data"];
			m_rigidbodies_data.m_forces.resize(m_rigidbodies_data.m_entity_map.size(), glm::vec3(0.0f));
			m_rigidbodies_data.m_torques.resize(m_rigidbodies_data.m_entity_map.size(), glm::vec3(0.0f));
			m_rigidbodies_data.m_inv_inertial_tensors.resize(m_rigidbodies_data.m_entity_map.size());
			for (size_t i = 0; i < m_rigidbodies_data.size(); i++)
				m_rigidbodies_data.m_inv_inertial_tensors[i] = glm::inverse(m_rigidbodies_data.m_inertial_tensors[i]);
		}
	}

	void RigidBodyManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["rigidbody_data"] = m_rigidbodies_data;
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

		m_entity_map.emplace(_entity, new_element_index);
		m_index_entities.push_back(_entity);

		// States
		// Linear
		m_positions.push_back(_position);
		m_velocities.push_back(glm::vec3(0.0f));
		m_forces.push_back(glm::vec3(0.0f));
		// Angular
		m_rotations.push_back(_rotation);
		m_angular_moments.push_back(glm::vec3(0.0f));
		m_torques.push_back(glm::vec3(0.0f));

		// Properties
		m_inv_masses.push_back(_mass);
		m_inertial_tensors.push_back(_inertial_tensor);
		m_inv_inertial_tensors.push_back(glm::inverse(_inertial_tensor));

		// Update enabled & disabled linear integration partitions.
		m_skip_linear_integration_count += 1;
		enable_linear_integration(_entity, true);

		return get_entity_index(_entity);
	}

	void RigidBodyManager::rigidbody_data_collection::erase(Entity _entity)
	{
		// If entity has linear integration enabled,
		// disable it so it is in front of disabled partition.
		enable_linear_integration(_entity, false);
		// Swap with back
		swap_back(size() - m_skip_linear_integration_count);
		pop_back();
		m_entity_map.erase(_entity);
	}

	bool RigidBodyManager::rigidbody_data_collection::has(Entity _entity) const
	{
		return m_entity_map.find(_entity) != m_entity_map.end();
	}

	size_t RigidBodyManager::rigidbody_data_collection::get_entity_index(Entity _entity) const
	{
		return m_entity_map.at(_entity);
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
		swap_indices(m_angular_moments);
		swap_indices(m_torques);
		// Constant properties
		swap_indices(m_inv_masses);
		swap_indices(m_inertial_tensors);
		swap_indices(m_inv_inertial_tensors);

		return true;
	}

	void RigidBodyManager::rigidbody_data_collection::pop_back()
	{
		m_index_entities.pop_back();

		m_positions.pop_back();
		m_velocities.pop_back();
		m_forces.pop_back();

		m_rotations.pop_back();
		m_angular_moments.pop_back();
		m_torques.pop_back();

		m_inv_masses.pop_back();
		m_inertial_tensors.pop_back();
		m_inv_inertial_tensors.pop_back();

		m_skip_linear_integration_count -= 1;
	}

	bool RigidBodyManager::rigidbody_data_collection::is_linear_integration_enabled(size_t const _entity_index) const
	{
		return _entity_index < (size() - m_skip_linear_integration_count);
	}

	void RigidBodyManager::rigidbody_data_collection::enable_linear_integration(Entity const _entity, bool _state)
	{
		size_t const set_entity_state_index = get_entity_index(_entity);
		bool is_enabled = is_linear_integration_enabled(set_entity_state_index);
		if (is_enabled && _state)
			return;

		// If we want to enable linear integration, swap to front of disabled partition
		// and decrease disabled partition size by one.
		if (!is_enabled && _state)
		{
			assert(m_skip_linear_integration_count > 0);
			size_t const front_disabled_index = size() - m_skip_linear_integration_count;
			Entity const swapped_entity = m_index_entities[front_disabled_index];

			swap_indices(set_entity_state_index, front_disabled_index);
			m_entity_map.at(_entity) = front_disabled_index;
			m_entity_map.at(swapped_entity) = set_entity_state_index;

			m_skip_linear_integration_count -= 1;
		}
		// If we want to disable linear integration, swap to back of enabled partition
		// and increase disabled partition size by one.
		else if (is_enabled && !_state)
		{
			assert(m_skip_linear_integration_count < size());
			size_t const back_enabled_index = size() - m_skip_linear_integration_count - 1;
			Entity const swapped_entity = m_index_entities[back_enabled_index];

			swap_indices(set_entity_state_index, back_enabled_index);
			m_entity_map.at(_entity) = back_enabled_index;
			m_entity_map.at(swapped_entity) = set_entity_state_index;

			m_skip_linear_integration_count += 1;
		}
	}

}