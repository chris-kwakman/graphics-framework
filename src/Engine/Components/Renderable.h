#ifndef COMPONENT_RENDERABLE_H
#define COMPONENT_RENDERABLE_H

#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/manager.h>

namespace Component
{
	using namespace Engine::ECS;
	using namespace Engine::Graphics;

	class RenderableManager;
	struct Renderable : public IComp<RenderableManager>
	{
		DECLARE_COMPONENT(Renderable);

		std::string		GetMeshName() const;
		mesh_handle		GetMeshHandle() const;
		void			SetMesh(mesh_handle _mesh);
	};

	class RenderableManager : public TCompManager<Renderable>
	{
		friend struct Renderable;

		std::unordered_map<Entity, mesh_handle, Entity::hash> m_mesh_map;

		void impl_clear() final;
		bool impl_create(Entity _e) final;
		void impl_destroy(Entity const* _entities, unsigned int _count) final;
		bool impl_component_owned_by_entity(Entity _entity) const final;
		void impl_edit_component(Entity _entity) final;
		void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) final;

	public:

		// Inherited via TCompManager
		const char* GetComponentTypeName() const final { return "Renderable"; }

		decltype(m_mesh_map) const& GetAllRenderables() const { return m_mesh_map; }

	};
}

#endif // COMPONENT_RENDERABLE_H