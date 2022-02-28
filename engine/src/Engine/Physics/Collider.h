#pragma once

#include <engine/ECS/component_manager.h>
#include <Engine/Physics/convex_hull.h>
#include <Engine/Physics/intersection.h>

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

		Engine::Physics::half_edge_data_structure const * GetConvexHull() const;
		void SetColliderResource(Engine::Managers::Resource _resource);
	};

	class ColliderManager : public TCompManager<Collider>
	{
		struct manager_data
		{
			struct ch_debug_render_instance
			{
				Engine::Managers::Resource m_collider_resource{};
				int m_highlight_face_index = -1;
				int m_highlight_edge_index = -1;

				NLOHMANN_DEFINE_TYPE_INTRUSIVE(ch_debug_render_instance, m_collider_resource, m_highlight_face_index, m_highlight_edge_index);
			};

			struct ch_debug_meshes
			{
				~ch_debug_meshes();

				unsigned int m_ref_count = 0;
				Engine::Graphics::mesh_handle m_ch_face_mesh = 0;
				Engine::Graphics::mesh_handle m_ch_edge_mesh = 0;
			};


			std::unordered_map<Entity, ch_debug_render_instance, Entity::hash> m_entity_map;
			std::map<Engine::Managers::Resource, ch_debug_meshes> m_ch_debug_meshes;

			std::map<std::pair<Entity,Entity>, std::pair<Engine::Physics::EIntersectionType, Engine::Physics::contact_manifold>> m_intersection_results;
			std::unordered_map<Entity, std::vector<Entity>, Entity::hash> m_entity_intersections;

			bool m_render_debug_face_mesh = true, m_render_debug_edge_mesh = true;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_entity_map, m_render_debug_face_mesh, m_render_debug_edge_mesh);
		};

	public:

		manager_data m_data;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		void SetColliderResource(Entity _e, Engine::Managers::Resource _resource);

		void TestColliderIntersections();

	private:

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};

	class PointHullCompManager;
	struct DebugPointHullComp : public IComp<PointHullCompManager>
	{
		DECLARE_COMPONENT(DebugPointHullComp);
	};

	class DebugPointHullCompManager : public TCompManager<DebugPointHullComp>
	{

		struct manager_data
		{
			struct entry_data
			{
				Engine::Managers::Resource m_pointhull_resource;
				Engine::Managers::Resource m_created_convexhull_resource;
				size_t m_convex_hull_creation_iterations = 0;

				NLOHMANN_DEFINE_TYPE_INTRUSIVE(entry_data, m_pointhull_resource, m_convex_hull_creation_iterations)
			};

			std::unordered_map<Entity, entry_data, Entity::hash> m_entity_map;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(manager_data, m_entity_map);
		};

		manager_data m_data;

	public:

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;

	private:

		void set_entity_pointhull_resource(Entity _e, Engine::Managers::Resource _ph_resource);
		void set_entity_pointhull_creation_iterations(Entity _e, size_t _iterations);
		Engine::Managers::Resource create_convex_hull_resource(Engine::Managers::Resource _ph_resource, size_t _iterations);
		void try_update_collider_resource(Entity _e);

		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}