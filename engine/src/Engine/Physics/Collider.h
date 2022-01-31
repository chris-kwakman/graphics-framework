#pragma once

#include <engine/ECS/component_manager.h>
#include <Engine/Physics/convex_hull.h>

#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Component
{
	using namespace Engine::ECS;

	class ColliderManager;
	struct Collider : public IComp<ColliderManager>
	{
		DECLARE_COMPONENT(Collider);
	};

	class ColliderManager : public TCompManager<Collider>
	{
		struct manager_data
		{
			std::unordered_map<Entity, Engine::Physics::convex_hull_handle, Entity::hash> m_entity_map;
			std::unordered_set<Entity, Entity::hash> m_renderables;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_entity_map, m_renderables);
		};

		manager_data m_data;

	public:

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;


		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}