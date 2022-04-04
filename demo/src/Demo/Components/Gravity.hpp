#include <engine/ECS/component_manager.h>
#include <unordered_set>

namespace Component
{
	using namespace Engine::ECS;

	struct GravityComponent : public IComp<GravityComponent>
	{
		DECLARE_COMPONENT(GravityComponent)
	};

	class GravityComponentManager : public TCompManager<GravityComponent>
	{
		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;

		struct manager_data
		{
			glm::vec3 m_gravity{ 0,-9.81f,0.0f };
			std::unordered_set<Entity, Entity::hash> m_gravity_entities;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_gravity, m_gravity_entities);
		};

		manager_data m_data;

	public:

		virtual const char* GetComponentTypeName() const override;
		void ApplyGravity();

	};
}