#include "Collider.h"
#include <Engine/Graphics/misc/create_convex_hull_mesh.h>
#include <Engine/Physics/point_hull.h>

#include <Engine/Components/Transform.h>

#include <Engine/Editor/editor.h>

namespace Component
{
	Engine::Physics::half_edge_data_structure const* Component::Collider::GetConvexHull() const
	{
		auto const handle = GetManager().m_data.m_entity_map[Owner()].Handle();
		return &Singleton<Engine::Physics::ConvexHullManager>().GetConvexHullInfo(handle)->m_data;
	}

	void Collider::SetColliderResource(Engine::Managers::Resource _resource)
	{
		assert(_resource.Type() == Singleton<Engine::Managers::ResourceManager>().find_named_type("Collider"));
		GetManager().SetColliderResource(Owner(), _resource);
	}

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
			if (debug_mesh_iter != m_data.m_ch_debug_meshes.end())
			{
				debug_mesh_iter->second.m_ref_count--;
				if (debug_mesh_iter->second.m_ref_count == 0)
					m_data.m_ch_debug_meshes.erase(debug_mesh_iter);
			}
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

		static bool display_debug_data = false;
		ImGui::Checkbox("Display Debug Data", &display_debug_data);
		if (display_debug_data)
		{
			Engine::Physics::DebugDisplayHalfEdgeDataStructure(
				&Singleton<ConvexHullManager>().GetConvexHullInfo(current_ch_handle)->m_data
			);
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




	void DebugPointHullCompManager::set_entity_pointhull_resource(Entity _e, Engine::Managers::Resource _ph_resource)
	{
		using namespace Engine::Managers;

		auto& res_mgr = Singleton<ResourceManager>();

		auto & entry = m_data.m_entity_map.at(_e);
		if (entry.m_pointhull_resource.ID() && entry.m_created_convexhull_resource.ID())
		{
			res_mgr.unload_resource(entry.m_created_convexhull_resource.ID());
			entry.m_pointhull_resource = Resource();
			entry.m_created_convexhull_resource = Resource();
		}
		entry.m_pointhull_resource = Resource();
		if (_ph_resource.ID())
		{
			using namespace Engine::Physics;

			entry.m_pointhull_resource = _ph_resource;
			entry.m_created_convexhull_resource = create_convex_hull_resource(entry.m_pointhull_resource, entry.m_convex_hull_creation_iterations);
		}
		try_update_collider_resource(_e);
	}
	void DebugPointHullCompManager::set_entity_pointhull_creation_iterations(Entity _e, size_t _iterations)
	{
		auto& entry = m_data.m_entity_map.at(_e);
		if (entry.m_created_convexhull_resource.ID())
		{
			Singleton<Engine::Managers::ResourceManager>().unload_resource(entry.m_created_convexhull_resource.ID());
			entry.m_created_convexhull_resource = create_convex_hull_resource(entry.m_pointhull_resource, _iterations);
			entry.m_convex_hull_creation_iterations = _iterations;
		}
		try_update_collider_resource(_e);
	}

	Engine::Managers::Resource DebugPointHullCompManager::create_convex_hull_resource(Engine::Managers::Resource _ph_resource, size_t _iterations)
	{
		using namespace Engine::Physics;
		using namespace Engine::Managers;

		auto& res_mgr = Singleton<ResourceManager>();

		if (_ph_resource.ID())
		{
			half_edge_data_structure new_hull = construct_convex_hull(_ph_resource.Handle(), _iterations);
			uint32_t const registered_hull_handle = Singleton<ConvexHullManager>().RegisterConvexHull(std::move(new_hull), "Debug Hull");
			resource_type const hull_collider_type = res_mgr.find_named_type("Collider");
			resource_id const ch_resource_id = res_mgr.register_resource(registered_hull_handle, hull_collider_type);
			return res_mgr.GetResourceFromID(ch_resource_id);
		}
		else
			return Resource();
	}

	void DebugPointHullCompManager::try_update_collider_resource(Entity _e)
	{
		Collider e_collider = _e.GetComponent<Collider>();
		if(e_collider.IsValid())
			e_collider.SetColliderResource(m_data.m_entity_map.at(_e).m_created_convexhull_resource);
	}


	const char* DebugPointHullCompManager::GetComponentTypeName() const
	{
		return "Debug Point Hull";
	}

	void DebugPointHullCompManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool DebugPointHullCompManager::impl_create(Entity _e)
	{
		m_data.m_entity_map.emplace(_e, manager_data::entry_data());
		return true;
	}
	void DebugPointHullCompManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			auto iter = m_data.m_entity_map.find(_entities[i]);
			if (iter != m_data.m_entity_map.end())
			{
				manager_data::entry_data& entry = iter->second;
				set_entity_pointhull_resource(_entities[i], Engine::Managers::Resource());
				m_data.m_entity_map.erase(_entities[i]);
			}
		}
	}
	bool DebugPointHullCompManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_data.m_entity_map.find(_entity) != m_data.m_entity_map.end();
	}
	void DebugPointHullCompManager::impl_edit_component(Entity _entity)
	{
		auto& entry = m_data.m_entity_map.at(_entity);
		if (Engine::Managers::resource_dragdrop_target(entry.m_pointhull_resource,"Point Hull"))
		{
			set_entity_pointhull_resource(_entity, entry.m_pointhull_resource);
		}
		int iterations = (int)entry.m_convex_hull_creation_iterations;
		if (ImGui::InputInt("Iterations", &iterations, 1, 5))
		{
			entry.m_convex_hull_creation_iterations = std::clamp(iterations, 0, std::numeric_limits<int>::max());
			set_entity_pointhull_creation_iterations(_entity, entry.m_convex_hull_creation_iterations);
		}
		
	}

	void DebugPointHullCompManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		if (_j.find("serializer_version") == _j.end())
			return;
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];

			for (auto & pair : m_data.m_entity_map)
			{
				auto& entry = pair.second;
				set_entity_pointhull_resource(pair.first, entry.m_pointhull_resource);
			}
		} 
	}

	void DebugPointHullCompManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
	}




	void ColliderManager::TestColliderIntersections()
	{
		using namespace Engine::Physics;

		m_data.m_intersection_results.clear();
		m_data.m_entity_intersections.clear();

		auto it1 = m_data.m_entity_map.begin();
		while (it1 != m_data.m_entity_map.end())
		{
			if (!it1->second.ID())
			{
				it1++;
				continue;
			}
			convex_hull_handle const ch_1 = it1->second.Handle();
			auto chi_1 = Singleton<ConvexHullManager>().GetConvexHullInfo(ch_1);
			auto it2 = it1;
			Engine::Math::transform3D const tr_1 = it1->first.GetComponent<Component::Transform>().ComputeWorldTransform();
			it2++;
			while (it2 != m_data.m_entity_map.end())
			{
				if (!it2->second.ID())
				{
					it2++;
					continue;
				}

				convex_hull_handle const ch_2 = it2->second.Handle();
				auto chi_2 = Singleton<ConvexHullManager>().GetConvexHullInfo(ch_2);
				Engine::Math::transform3D const tr_2 = it2->first.GetComponent<Component::Transform>().ComputeWorldTransform();
				auto result = intersect_convex_hulls_sat(chi_1->m_data, tr_1, chi_2->m_data, tr_2);
				if (result.intersection_type & EIntersectionType::eAnyIntersection)
				{
					m_data.m_intersection_results.emplace(
						std::pair(it1->first, it2->first), result
					);
					m_data.m_entity_intersections[it1->first].emplace_back(it2->first);
					m_data.m_entity_intersections[it2->first].emplace_back(it1->first);
				}
				it2++;
			}
			it1++;
		}
	}
}