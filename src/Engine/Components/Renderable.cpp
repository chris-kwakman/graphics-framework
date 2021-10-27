#include "Renderable.h"
#include "Transform.h"
#include <Engine/Utils/logging.h>
#include <ImGui/imgui.h>

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

		mesh_handle payload_mesh_handle;

		auto accept_mesh_handle_payload = [&payload_mesh_handle]()->bool
		{
			bool result = false;
			if (ImGui::BeginDragDropTarget())
			{
				if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("RESOURCE_MESH"))
				{
					payload_mesh_handle = *(mesh_handle*)payload->Data;
					result = true;
				}

				ImGui::EndDragDropTarget();
			}
			return result;
		};

		bool mesh_payload_accepted = false;
		ImGui::InputText("Mesh Name", (char*)mesh_name.c_str(), mesh_name.size(), ImGuiInputTextFlags_ReadOnly);
		mesh_payload_accepted |= accept_mesh_handle_payload();
		//ImGui::InputInt("Mesh ID", (int*)&my_mesh, 0, 0, ImGuiInputTextFlags_ReadOnly);
		//mesh_payload_accepted |= accept_mesh_handle_payload();

		if (mesh_payload_accepted)
			component.SetMesh(payload_mesh_handle);
	}

	void RenderableManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
		auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		Renderable component = Get(_e);
		auto mesh_handle = resource_manager.FindMesh(_json_comp["mesh_name"].get<std::string>().c_str());
		component.SetMesh(mesh_handle);
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

}