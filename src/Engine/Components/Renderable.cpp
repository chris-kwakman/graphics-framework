#include "Renderable.h"
#include "Transform.h"
#include <Engine/Utils/logging.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>

namespace Component
{
	static std::string get_mesh_name(mesh_handle _mesh)
	{
		auto& named_mesh_map = Singleton<ResourceManager>().m_named_mesh_map;
		if (_mesh == 0)
			return "No Mesh";
		for (auto pair : named_mesh_map)
		{
			if (pair.second == _mesh)
				return pair.first;
		}
		return "Unnamed";
	}

	static std::string get_texture_name(texture_handle _texture)
	{
		auto& filepath_texture_map = Singleton<ResourceManager>().m_filepath_texture_map;
		if (_texture == 0)
			return "No Texture";
		for (auto pair : filepath_texture_map)
		{
			if (pair.second == _texture)
				return pair.first;
		}
		return "Unnamed";
	}

	template<typename resource_handle>
	resource_handle accept_resource_handle_payload(const char * _resource_label)
	{
		resource_handle dropped_handle = 0;
		if (ImGui::BeginDragDropTarget())
		{
			if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload(_resource_label))
			{
				dropped_handle = *(resource_handle*)payload->Data;
			}

			ImGui::EndDragDropTarget();
		}
		return dropped_handle;
	};

	///////////////////////////////////////////////////////////////////////////
	//					Renderable & Renderable Manager
	///////////////////////////////////////////////////////////////////////////

	std::string Renderable::GetMeshName() const
	{
		return get_mesh_name(GetMeshHandle());
	}

	mesh_handle Renderable::GetMeshHandle() const
	{
		return GetManager().m_mesh_map.at(m_owner);
	}

	void Renderable::SetMesh(mesh_handle _mesh)
	{
		GetManager().m_mesh_map.find(m_owner)->second = _mesh;
	}




	void RenderableManager::impl_clear()
	{
		m_mesh_map.clear();
	}

	bool RenderableManager::impl_create(Entity _e)
	{
		// Renderable has transform dependency (how can we render without a position?)
		if(!_e.HasComponent<Transform>())
			return false;

		m_mesh_map.emplace(_e, 0);

		return true;
	}

	void RenderableManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; i++)
		{
			m_mesh_map.erase(_entities[i]);
		}
	}

	bool RenderableManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_mesh_map.find(_entity) != m_mesh_map.end();
	}

	void RenderableManager::impl_edit_component(Entity _entity)
	{
		auto component = Get(_entity);
		std::string mesh_name = component.GetMeshName();
		mesh_handle my_mesh = component.GetMeshHandle();


		mesh_handle payload_mesh_handle = 0;
		ImGui::InputText("Mesh Name", (char*)mesh_name.c_str(), mesh_name.size(), ImGuiInputTextFlags_ReadOnly);
		payload_mesh_handle |= accept_resource_handle_payload<mesh_handle>("RESOURCE_MESH");
		//ImGui::InputInt("Mesh ID", (int*)&my_mesh, 0, 0, ImGuiInputTextFlags_ReadOnly);
		//mesh_payload_accepted |= accept_mesh_handle_payload();

		if (payload_mesh_handle)
			component.SetMesh(payload_mesh_handle);
	}

	void RenderableManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
		auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		Renderable component = Get(_e);
		auto mesh_handle = resource_manager.FindMesh(_json_comp["mesh_name"].get<std::string>().c_str());
		component.SetMesh(mesh_handle);
	}

	///////////////////////////////////////////////////////////////////////////
	//						Skin & Skin Manager
	///////////////////////////////////////////////////////////////////////////

	skin_handle Skin::GetSkinHandle() const
	{
		return GetManager().m_skin_instance_map.at(m_owner).m_skin_handle;
	}

	void Skin::SetSkin(skin_handle _skin)
	{
		GetManager().m_skin_instance_map.at(m_owner).m_skin_handle = _skin;
	}

	Transform Skin::GetSkeletonRootNode() const
	{
		return GetManager().m_skin_instance_map.at(m_owner).m_skeleton_root;
	}

	void Skin::SetSkeletonRootNode(Transform _node)
	{
		GetManager().m_skin_instance_map.at(m_owner).m_skeleton_root = _node;
	}

	bool Skin::ShouldRenderJoints() const
	{
		return GetManager().m_skin_instance_map.at(m_owner).m_render_joints;
	}

	void Skin::SetShouldRenderJoints(bool _state)
	{
		GetManager().m_skin_instance_map.at(m_owner).m_render_joints = _state;
	}

	std::vector<Transform> const& Skin::GetSkeletonInstanceNodes() const
	{
		return GetManager().m_skin_instance_map.at(m_owner).m_skeleton_instance_nodes;
	}

	void Skin::SetSkeletonInstanceNodes(std::vector<Transform> const& _nodes) const
	{
		GetManager().m_skin_instance_map.at(m_owner).m_skeleton_instance_nodes = _nodes;
	}

	std::vector<glm::mat4x4> const & Skin::GetSkinNodeInverseBindMatrices() const
	{
		auto& res_mgr = Singleton<Engine::Graphics::ResourceManager>();
		return res_mgr.m_skin_data_map.at(GetSkinHandle()).m_inv_bind_matrices;
	}


	const char* SkinManager::GetComponentTypeName() const
	{
		return "Skin";
	}

	void SkinManager::impl_clear()
	{
		m_skin_instance_map.clear();
	}

	bool SkinManager::impl_create(Entity _e)
	{
		skin_instance new_skin_instance;
		new_skin_instance.m_skin_handle = 0;
		new_skin_instance.m_skeleton_instance_nodes.clear();
		new_skin_instance.m_render_joints = false;
		m_skin_instance_map.emplace(_e, std::move(new_skin_instance));
		return true;
	}

	void SkinManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
			m_skin_instance_map.erase(_entities[i]);
	}

	bool SkinManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_skin_instance_map.find(_entity) != m_skin_instance_map.end();
	}

	void SkinManager::impl_edit_component(Entity _entity)
	{
		skin_instance & instance = m_skin_instance_map.at(_entity);
		ImGui::Checkbox("Render Joints", &instance.m_render_joints);
		ImGui::BeginListBox("Joint Nodes");
		for (Transform const joint : instance.m_skeleton_instance_nodes)
		{
			bool selected = false;
			ImGui::Selectable(joint.Owner().GetName(), &selected);
		}
		ImGui::EndListBox();
	}

	void SkinManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	///////////////////////////////////////////////////////////////////////////
	//						Decals & Decal Manager
	///////////////////////////////////////////////////////////////////////////

	using E_DecalRenderMode = DecalManager::E_DecalRenderMode;
	E_DecalRenderMode DecalManager::s_decal_render_mode = E_DecalRenderMode::eDecal;
	static std::unordered_map<E_DecalRenderMode, const char*> const s_decal_render_mode_names{
		{E_DecalRenderMode::eDecal, "Decal"},
		{E_DecalRenderMode::eDecalMask, "Decal Mask"},
		{E_DecalRenderMode::eDecalBoundingVolume, "Decal Bounding Volume"},
	};
	float DecalManager::s_decal_angle_treshhold = 89.7f;
	bool DecalManager::s_render_decals = true;

	const char* DecalManager::GetComponentTypeName() const
	{
		return "Decal";
	}

	decltype(DecalManager::m_decal_data_map) const& DecalManager::GetAllDecals() const
	{
		return m_decal_data_map;
	}

	void DecalManager::impl_clear()
	{
		m_decal_data_map.clear();
	}

	bool DecalManager::impl_create(Entity _e)
	{
		if (impl_component_owned_by_entity(_e))
			return true;

		m_decal_data_map.emplace(_e, decal_textures{});
		return true;
	}

	void DecalManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
			m_decal_data_map.erase(_entities[i]);
	}

	bool DecalManager::impl_component_owned_by_entity(Entity _entity) const
	{
		auto iter = m_decal_data_map.find(_entity);
		return iter != m_decal_data_map.end();
	}

	void DecalManager::impl_edit_component(Entity _entity)
	{
		decal_textures& decal_textures = m_decal_data_map.at(_entity);
		std::string texture_name_albedo, texture_name_metallic_roughness, texture_name_normal;
		texture_handle texture_payload = 0;

		texture_name_albedo				= get_texture_name(decal_textures.m_texture_albedo);
		texture_name_metallic_roughness = get_texture_name(decal_textures.m_texture_metallic_roughness);
		texture_name_normal				= get_texture_name(decal_textures.m_texture_normal);

		ImGui::InputText("Albedo", &texture_name_albedo, ImGuiInputTextFlags_ReadOnly);
		texture_payload = accept_resource_handle_payload<texture_handle>("RESOURCE_TEXTURE");
		if (texture_payload)
			decal_textures.m_texture_albedo = texture_payload;

		ImGui::InputText("Metallic Roughness", &texture_name_metallic_roughness, ImGuiInputTextFlags_ReadOnly);
		texture_payload = accept_resource_handle_payload<texture_handle>("RESOURCE_TEXTURE");
		if (texture_payload)
			decal_textures.m_texture_metallic_roughness = texture_payload;

		ImGui::InputText("Normal", &texture_name_normal, ImGuiInputTextFlags_ReadOnly);
		texture_payload = accept_resource_handle_payload<texture_handle>("RESOURCE_TEXTURE");
		if (texture_payload)
			decal_textures.m_texture_normal = texture_payload;

		ImGui::Separator();

		ImGui::Checkbox("Enable Decal Rendering", &s_render_decals);
		if(ImGui::BeginCombo("Decal Render Mode", s_decal_render_mode_names.at(s_decal_render_mode)))
		{
			for (unsigned int i = 0; i < E_DecalRenderMode::COUNT; ++i)
			{
				bool selected = ((int)s_decal_render_mode == i);
				if (ImGui::Selectable(s_decal_render_mode_names.at((E_DecalRenderMode)i), &selected))
					s_decal_render_mode = (E_DecalRenderMode)i;
			}
			ImGui::EndCombo();
		}
		ImGui::SliderFloat("Decal Angle Treshhold", &s_decal_angle_treshhold, 0.0f, 90.0f, "%.1f");
	}

	void DecalManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	decal_textures& Decal::GetTexturesRef()
	{
		return GetManager().m_decal_data_map.at(Owner());
	}

	decal_textures Decal::GetTextures() const
	{
		return GetManager().m_decal_data_map.at(Owner());
	}

	float Decal::GetAngleTreshhold() const
	{
		return GetManager().s_decal_angle_treshhold;
	}

}