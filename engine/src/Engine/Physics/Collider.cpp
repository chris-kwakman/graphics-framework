#include "Collider.h"
#include <Engine/Graphics/misc/create_convex_hull_mesh.h>

#include <ImGui/imgui_stdlib.h>

namespace Component
{
	const char* ColliderManager::GetComponentTypeName() const
	{
		return "Collider";
	}
	void ColliderManager::SetColliderConvexHull(Entity _e, Engine::Physics::convex_hull_handle _ch_handle)
	{
		auto& ch_mgr = Singleton<Engine::Physics::ConvexHullManager>();

		if (!ComponentOwnedByEntity(_e))
			return;

		auto map_iter = m_data.m_entity_map.find(_e);
		Engine::Physics::convex_hull_handle const old_handle = map_iter->second;
		if (old_handle != 0)
		{
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(old_handle);
			debug_mesh_iter->second.m_ref_count--;
			if (debug_mesh_iter->second.m_ref_count == 0)
				m_data.m_ch_debug_meshes.erase(debug_mesh_iter);
		}
		map_iter->second = _ch_handle;
		if (_ch_handle != 0)
		{
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(_ch_handle);
			if (debug_mesh_iter == m_data.m_ch_debug_meshes.end())
			{
				manager_data::ch_debug_render_data render_data;
				render_data.m_ch_mesh = Engine::Graphics::Misc::create_convex_hull_mesh(_ch_handle);
				render_data.m_ref_count = 0;
				m_data.m_ch_debug_meshes.emplace(_ch_handle, std::move(render_data));
			}
			m_data.m_ch_debug_meshes.at(_ch_handle).m_ref_count++;
		}
	}
	void ColliderManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool ColliderManager::impl_create(Entity _e)
	{
		m_data.m_entity_map.emplace(_e, 0);

		return true;
	}
	void ColliderManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			Entity const destroy_entity = _entities[i];
			auto map_iter = m_data.m_entity_map.find(destroy_entity);
			if (map_iter != m_data.m_entity_map.end())
			{
				m_data.m_renderables.erase(destroy_entity);
				m_data.m_entity_map.erase(map_iter);
			}
		}
	}
	bool ColliderManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_data.m_entity_map.find(_entity) != m_data.m_entity_map.end();
	}
	void ColliderManager::impl_edit_component(Entity _entity)
	{
		using namespace Engine::Physics;

		auto& ch_mgr = Singleton<ConvexHullManager>();

		auto renderable_iter = m_data.m_renderables.find(_entity);
		bool is_rendered = (renderable_iter != m_data.m_renderables.end());
		if (ImGui::Checkbox("Debug Render", &is_rendered))
		{
			if (!is_rendered)
				m_data.m_renderables.erase(renderable_iter);
			else
				m_data.m_renderables.emplace(_entity);
		}
		auto info = ch_mgr.GetConvexHullInfo(m_data.m_entity_map.at(_entity));
		std::string show_name = "No Collider";
		if (info != nullptr)
		{
			show_name = info->m_name;
		}
		ImGui::InputText("Collider Handle", &show_name, ImGuiInputTextFlags_ReadOnly);

		if (ImGui::Button("Set Collider"))
			ImGui::OpenPopup("convex_hull_list");

		if (ImGui::BeginPopupContextItem("convex_hull_list"))
		{
			Engine::Physics::convex_hull_handle new_handle = 0;
			auto map_iter = ch_mgr.m_map.begin();
			while (map_iter != ch_mgr.m_map.end())
			{
				if (ImGui::MenuItem(map_iter->second.m_name.c_str()))
				{
					new_handle = map_iter->first;
					ImGui::CloseCurrentPopup();
				}

				map_iter++;
			}

			SetColliderConvexHull(_entity, new_handle);

			ImGui::EndPopup();
		}
	}
	void ColliderManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];
		}
	}
	void ColliderManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
	}
}