#include "Renderable.h"
#include "Transform.h"
#include <Engine/Utils/logging.h>
#include <ImGui/imgui.h>

namespace Component
{
	std::string Renderable::GetMeshName() const
	{
		auto& named_mesh_map = Singleton<ResourceManager>().m_named_mesh_map;
		ResourceManager::mesh_handle my_mesh_handle = GetMeshHandle();
		if (my_mesh_handle == 0)
			return "No Mesh";
		for (auto pair : named_mesh_map)
		{
			if (pair.second == my_mesh_handle)
				return pair.first;
		}
		return "Unnamed";
	}

	ResourceManager::mesh_handle Renderable::GetMeshHandle() const
	{
		return get_manager().m_mesh_map.at(m_owner);
	}

	void Renderable::SetMesh(ResourceManager::mesh_handle _mesh)
	{
		get_manager().m_mesh_map.find(m_owner)->second = _mesh;
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
		ResourceManager::mesh_handle mesh_handle = component.GetMeshHandle();

		ImGui::InputText("Mesh Name", (char*)mesh_name.c_str(), mesh_name.size(), ImGuiInputTextFlags_ReadOnly);
		ImGui::InputInt("Mesh ID", (int*)&mesh_handle, 0, 0, ImGuiInputTextFlags_ReadOnly);
	}

}