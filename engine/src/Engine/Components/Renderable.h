#ifndef COMPONENT_RENDERABLE_H
#define COMPONENT_RENDERABLE_H

#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/manager.h>

#include "Transform.h"

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

	class SkinManager;
	struct Skin : public IComp<SkinManager>
	{
		DECLARE_COMPONENT(Skin);

		skin_handle		GetSkinHandle() const;
		void			SetSkin(skin_handle _skin);

		Transform		GetSkeletonRootNode() const;
		void			SetSkeletonRootNode(Transform _node);

		bool			ShouldRenderJoints() const;
		void			SetShouldRenderJoints(bool _state);

		std::vector<Transform> const &	GetSkeletonInstanceNodes() const;
		void SetSkeletonInstanceNodes(std::vector<Transform> const & _nodes) const;

		std::vector<glm::mat4x4> const & GetSkinNodeInverseBindMatrices() const;

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

	public:

		// Inherited via TCompManager
		const char* GetComponentTypeName() const final { return "Renderable"; }

		decltype(m_mesh_map) const& GetAllRenderables() const { return m_mesh_map; }


		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;

	};

	class SkinManager : public TCompManager<Skin>
	{
		friend struct Skin;

		struct skin_instance
		{
			skin_handle				m_skin_handle;
			Transform				m_skeleton_root;
			std::vector<Transform>	m_skeleton_instance_nodes;
			bool					m_render_joints;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(skin_instance, m_skin_handle, m_skeleton_root, m_skeleton_instance_nodes)
		};

		std::unordered_map<Entity, skin_instance, Entity::hash> m_skin_instance_map;

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

	public:

		virtual const char* GetComponentTypeName() const override;

		decltype(m_skin_instance_map) const& GetAllSkinEntities() const { return m_skin_instance_map; }


		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;

	};

	struct decal_textures
	{
		texture_handle	m_texture_albedo,
						m_texture_metallic_roughness,
						m_texture_normal;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(decal_textures, m_texture_albedo, m_texture_metallic_roughness, m_texture_normal)
	};

	class DecalManager;
	struct Decal : public IComp<DecalManager>
	{
		DECLARE_COMPONENT(Decal);

		decal_textures& GetTexturesRef();
		decal_textures GetTextures() const;

		float			GetAngleTreshhold() const;
	};

	class DecalManager final : public TCompManager<Decal>
	{
		friend struct Decal;

		std::unordered_map<Entity, decal_textures, Entity::hash> m_decal_data_map;

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

	public:

		enum E_DecalRenderMode {eDecalBoundingVolume, eDecalMask, eDecal, COUNT};
		static E_DecalRenderMode	s_decal_render_mode;
		static float				s_decal_angle_treshhold;
		static bool					s_render_decals;

		virtual const char* GetComponentTypeName() const override;
		decltype(m_decal_data_map) const& GetAllDecals() const;
		E_DecalRenderMode GetDecalRenderMode() const { return s_decal_render_mode; }

		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}

#endif // COMPONENT_RENDERABLE_H