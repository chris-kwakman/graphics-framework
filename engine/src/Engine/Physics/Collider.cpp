#include "Collider.h"
#include <Engine/Graphics/misc/create_convex_hull_mesh.h>

#include <Engine/Editor/editor.h>

namespace Component
{
	const char* ColliderManager::GetComponentTypeName() const
	{
		return "Collider";
	}
	void ColliderManager::SetColliderResource(Entity _e, Engine::Managers::Resource _resource)
	{
		auto& ch_mgr = Singleton<Engine::Physics::ConvexHullManager>();

		if (!ComponentOwnedByEntity(_e))
			return;

		Engine::Physics::convex_hull_handle const input_resource_handle = _resource.Handle();

		auto map_iter = m_data.m_entity_map.find(_e);
		Engine::Physics::convex_hull_handle const old_handle = map_iter->second.Handle();
		if (old_handle != 0)
		{
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(old_handle);
			debug_mesh_iter->second.m_ref_count--;
			if (debug_mesh_iter->second.m_ref_count == 0)
				m_data.m_ch_debug_meshes.erase(debug_mesh_iter);
		}
		map_iter->second = _resource;
		if (_resource.ID() != 0)
		{
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(_resource.Handle());
			if (debug_mesh_iter == m_data.m_ch_debug_meshes.end())
			{
				manager_data::ch_debug_render_data render_data;
				render_data.m_ch_face_mesh = Engine::Graphics::Misc::create_convex_hull_face_mesh(input_resource_handle);
				render_data.m_ch_edge_mesh = Engine::Graphics::Misc::create_convex_hull_edge_mesh(input_resource_handle);
				render_data.m_ref_count = 0;
				m_data.m_ch_debug_meshes.emplace(input_resource_handle, std::move(render_data));
			}
			m_data.m_ch_debug_meshes.at(input_resource_handle).m_ref_count++;
		}
	}
	void ColliderManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool ColliderManager::impl_create(Entity _e)
	{
		m_data.m_entity_map.emplace(_e, Engine::Managers::Resource());

		return true;
	}
	void ColliderManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			Entity const destroy_entity = _entities[i];
			auto map_iter = m_data.m_entity_map.find(destroy_entity);
			SetColliderResource(destroy_entity, Engine::Managers::Resource());
			m_data.m_entity_map.erase(destroy_entity);
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

		auto info = ch_mgr.GetConvexHullInfo(m_data.m_entity_map.at(_entity).Handle());
		std::string show_name = "No Collider";
		if (info != nullptr)
		{
			show_name = info->m_name;
		}
		ImGui::InputText("Collider Handle", &show_name, ImGuiInputTextFlags_ReadOnly);

		ImGui::Checkbox("Enable Debug Face Rendering", &m_data.m_render_debug_face_mesh);
		ImGui::Checkbox("Enable Debug Edge Rendering", &m_data.m_render_debug_edge_mesh);

		Engine::Managers::Resource current_resource = m_data.m_entity_map.at(_entity);
		if (Engine::Managers::resource_dragdrop_target(current_resource, "Collider"))
			SetColliderResource(_entity, current_resource);

		convex_hull_handle const current_ch_handle = m_data.m_entity_map.at(_entity).Handle();
		if (current_ch_handle != 0)
		{
			if (auto it = m_data.m_ch_debug_meshes.find(current_ch_handle); it != m_data.m_ch_debug_meshes.end())
			{
				auto& debug_render_data = it->second;
				auto const & face_prim_list = Singleton<Engine::Graphics::ResourceManager>().GetMeshPrimitives(debug_render_data.m_ch_face_mesh);
				auto const & edge_prim_list = Singleton<Engine::Graphics::ResourceManager>().GetMeshPrimitives(debug_render_data.m_ch_edge_mesh);
				auto const & ch_data = Singleton<Engine::Physics::ConvexHullManager>().GetConvexHullInfo(current_ch_handle);
				ImGui::Separator();
				ImGui::SliderInt("Highlight Face Index",
					&debug_render_data.m_highlight_face_index, -1,
					ch_data->m_data.m_faces.size()-1,
					"%d",
					ImGuiSliderFlags_AlwaysClamp
				);
				ImGui::SliderInt("Highlight Edge Index",
					&debug_render_data.m_highlight_edge_index, -1,
					ch_data->m_data.m_edges.size() - 1,
					"%d",
					ImGuiSliderFlags_AlwaysClamp
				);
			}

		}
	}
	void ColliderManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];
		}

		// Create meshes for convex hulls if they do not exist yet.
		std::unordered_map<Engine::Physics::convex_hull_handle, unsigned int> map_ch_refcount;
		for (auto [e, resource] : m_data.m_entity_map)
		{
			Engine::Physics::convex_hull_handle const resource_handle = resource.Handle();
			map_ch_refcount.try_emplace(resource_handle, 0);
			map_ch_refcount.at(resource_handle)++;
		}
		for (auto [ch_handle, refcount] : map_ch_refcount)
		{
			manager_data::ch_debug_render_data ch_debug_data;
			ch_debug_data.m_ref_count = refcount;
			ch_debug_data.m_ch_edge_mesh = Engine::Graphics::Misc::create_convex_hull_edge_mesh(ch_handle);
			ch_debug_data.m_ch_face_mesh = Engine::Graphics::Misc::create_convex_hull_face_mesh(ch_handle);
			m_data.m_ch_debug_meshes.emplace(ch_handle, ch_debug_data);
		}
	}
	void ColliderManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
	}

	ColliderManager::manager_data::ch_debug_render_data::~ch_debug_render_data()
	{
		using namespace Engine::Graphics;
		auto& rm = Singleton<ResourceManager>();

		// Delete graphics resources.
	}
}