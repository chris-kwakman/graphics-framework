#include "Gravity.hpp"
#include <Engine/Components/Rigidbody.h>

namespace Component
{
	const char* GravityComponentManager::GetComponentTypeName() const
	{
		return "GravityComponent";
	}
	void GravityComponentManager::ApplyGravity()
	{
		for (auto entity : m_data.m_gravity_entities)
		{
			auto rb_comp = entity.GetComponent<RigidBody>();
			auto rb_data = rb_comp.GetRigidBodyData();
			rb_data.force_accumulator += m_data.m_gravity * rb_data.get_mass();
			rb_comp.SetRigidBodyData(rb_data);
		}
	}
	void GravityComponentManager::impl_clear()
	{
		m_data = manager_data{};
	}
	bool GravityComponentManager::impl_create(Entity _e)
	{
		bool const is_alive = _e.Alive();
		if (is_alive)
			m_data.m_gravity_entities.emplace(_e);
		return is_alive;
	}
	void GravityComponentManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
			m_data.m_gravity_entities.erase(_entities[i]);
	}
	bool GravityComponentManager::impl_component_owned_by_entity(Entity _entity) const
	{
		auto const iter = m_data.m_gravity_entities.find(_entity);
		return iter != m_data.m_gravity_entities.end();
	}
	void GravityComponentManager::impl_edit_component(Entity _entity)
	{
		ImGui::DragFloat3("Global Gravity", &m_data.m_gravity.x, 0.1f);
	}
	void GravityComponentManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];
		}
	}
	void GravityComponentManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;
		_j["m_data"] = m_data;
	}
}