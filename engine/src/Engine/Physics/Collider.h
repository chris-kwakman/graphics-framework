#pragma once

#include <engine/ECS/component_manager.h>
#include <Engine/Physics/convex_hull.h>

#include <Engine/Graphics/manager.h>

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
			struct ch_debug_render_data
			{
				~ch_debug_render_data();

				unsigned int m_ref_count = 0;
				Engine::Graphics::mesh_handle m_ch_face_mesh;
				Engine::Graphics::mesh_handle m_ch_edge_mesh;
				int m_highlight_face_index = -1;
				int m_highlight_edge_index = -1;
			};

			std::unordered_map<Entity, Engine::Managers::Resource, Entity::hash> m_entity_map;
			std::unordered_map<Engine::Physics::convex_hull_handle, ch_debug_render_data> m_ch_debug_meshes;

			bool m_render_debug_face_mesh = true, m_render_debug_edge_mesh = true;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_entity_map, m_render_debug_face_mesh, m_render_debug_edge_mesh);
		};

	public:

		manager_data m_data;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		void SetColliderResource(Entity _e, Engine::Managers::Resource _resource);

	private:

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}