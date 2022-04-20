#include "Collider.h"
#include <Engine/Graphics/misc/create_convex_hull_mesh.h>
#include <Engine/Physics/point_hull.h>
#include <Engine/Components/Rigidbody.h>
#include <Engine/Components/Transform.h>

#include <Engine/Physics/contact.h>
#include <Engine/Physics/resolution.hpp>

#include <Engine/Editor/editor.h>

namespace Component
{
	using namespace Engine::Physics;

	Engine::Physics::half_edge_data_structure const* Component::Collider::GetConvexHull() const
	{
		auto const handle = GetManager().m_data.m_entity_map[Owner()].m_collider_resource.Handle();
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

		auto map_iter = m_data.m_entity_map.find(_e);
		Engine::Managers::Resource const collider_resource = map_iter->second.m_collider_resource;
		if (collider_resource.ID() != 0)
		{
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(collider_resource);
			if (debug_mesh_iter != m_data.m_ch_debug_meshes.end())
			{
				debug_mesh_iter->second.m_ref_count--;
				if (debug_mesh_iter->second.m_ref_count == 0)
					m_data.m_ch_debug_meshes.erase(debug_mesh_iter);
			}
		}
		map_iter->second.m_collider_resource = _resource;
		if (_resource.ID() != 0)
		{
			Engine::Physics::convex_hull_handle const input_resource_handle = _resource.Handle();
			auto debug_mesh_iter = m_data.m_ch_debug_meshes.find(_resource);
			if (debug_mesh_iter == m_data.m_ch_debug_meshes.end())
			{
				manager_data::ch_debug_meshes render_data;
				render_data.m_ch_face_mesh = Engine::Graphics::Misc::create_convex_hull_face_mesh(input_resource_handle);
				render_data.m_ch_edge_mesh = Engine::Graphics::Misc::create_convex_hull_edge_mesh(input_resource_handle);
				render_data.m_ref_count = 0;
				m_data.m_ch_debug_meshes.emplace(_resource, std::move(render_data));
			}
			m_data.m_ch_debug_meshes.at(_resource).m_ref_count++;
		}
	}
	void ColliderManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool ColliderManager::impl_create(Entity _e)
	{
		m_data.m_entity_map.emplace(_e, manager_data::ch_debug_render_instance{});

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
		auto& debug_render_instance = m_data.m_entity_map.at(_entity);
		Engine::Managers::Resource current_resource = debug_render_instance.m_collider_resource;

		auto info = ch_mgr.GetConvexHullInfo(current_resource.Handle());
		std::string show_name = "No Collider";
		if (info != nullptr)
		{
			show_name = info->m_name;
		}
		ImGui::InputText("Collider Handle", &show_name, ImGuiInputTextFlags_ReadOnly);

		ImGui::Checkbox("Enable Debug Face Rendering", &m_data.m_render_debug_face_mesh);
		ImGui::Checkbox("Enable Debug Edge Rendering", &m_data.m_render_debug_edge_mesh);

		if (Engine::Managers::resource_dragdrop_target(current_resource, "Collider"))
			SetColliderResource(_entity, current_resource);

		convex_hull_handle const current_ch_handle = current_resource.Handle();
		if (current_resource.ID() != 0 && current_ch_handle != 0)
		{
			if (auto it = m_data.m_ch_debug_meshes.find(current_resource); it != m_data.m_ch_debug_meshes.end())
			{
				auto& debug_render_meshes = it->second;
				auto const & face_prim_list = Singleton<Engine::Graphics::ResourceManager>().GetMeshPrimitives(debug_render_meshes.m_ch_face_mesh);
				auto const & edge_prim_list = Singleton<Engine::Graphics::ResourceManager>().GetMeshPrimitives(debug_render_meshes.m_ch_edge_mesh);
				auto const & ch_data = Singleton<Engine::Physics::ConvexHullManager>().GetConvexHullInfo(current_ch_handle);
				ImGui::Separator();
				ImGui::SliderInt("Highlight Face Index",
					&debug_render_instance.m_highlight_face_index, -1,
					ch_data->m_data.m_faces.size()-1,
					"%d",
					ImGuiSliderFlags_AlwaysClamp
				);
				ImGui::SliderInt("Highlight Edge Index",
					&debug_render_instance.m_highlight_edge_index, -1,
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
		std::map<Engine::Managers::Resource, unsigned int> map_collider_res_refcount;
		for (auto [e, debug_render_instance] : m_data.m_entity_map)
		{
			Engine::Managers::Resource const collider_resource = debug_render_instance.m_collider_resource;
			map_collider_res_refcount.try_emplace(collider_resource, 0);
			map_collider_res_refcount.at(collider_resource) += 1;
		}
		for (auto [resource, refcount] : map_collider_res_refcount)
		{
			manager_data::ch_debug_meshes ch_debug_data;
			ch_debug_data.m_ref_count = refcount;
			ch_debug_data.m_ch_edge_mesh = Engine::Graphics::Misc::create_convex_hull_edge_mesh(resource.Handle());
			ch_debug_data.m_ch_face_mesh = Engine::Graphics::Misc::create_convex_hull_face_mesh(resource.Handle());
			m_data.m_ch_debug_meshes.emplace(resource, ch_debug_data);
		}
	}
	void ColliderManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
	}

	ColliderManager::manager_data::ch_debug_meshes::~ch_debug_meshes()
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
		int resolution_iterations_penetration = (int)entry.m_convex_hull_creation_iterations;
		if (ImGui::InputInt("Iterations", &resolution_iterations_penetration, 1, 5))
		{
			entry.m_convex_hull_creation_iterations = std::clamp(resolution_iterations_penetration, 0, std::numeric_limits<int>::max());
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

		global_contact_data& mgr_global_contact_data = m_data.m_global_contact_data;
		mgr_global_contact_data.all_contacts.clear();
		mgr_global_contact_data.all_contact_manifolds.clear();
		mgr_global_contact_data.debug_draw_lines.clear();
		mgr_global_contact_data.debug_draw_points.clear();

		std::array<contact, 128>	contact_stack_arr;
		size_t						contact_stack_size = 0;

		auto it1 = m_data.m_entity_map.begin();
		while (it1 != m_data.m_entity_map.end())
		{
			if (!it1->second.m_collider_resource.ID())
			{
				it1++;
				continue;
			}
			convex_hull_handle const ch_1 = it1->second.m_collider_resource.Handle();
			auto chi_1 = Singleton<ConvexHullManager>().GetConvexHullInfo(ch_1);
			auto it2 = it1;
			Engine::Math::transform3D const tr_1 = it1->first.GetComponent<Component::Transform>().ComputeWorldTransform();
			it2++;
			while (it2 != m_data.m_entity_map.end())
			{
				if (!it2->second.m_collider_resource.ID())
				{
					it2++;
					continue;
				}

				contact_stack_size = 0;

				convex_hull_handle const ch_2 = it2->second.m_collider_resource.Handle();
				auto chi_2 = Singleton<ConvexHullManager>().GetConvexHullInfo(ch_2);
				Engine::Math::transform3D const tr_2 = it2->first.GetComponent<Component::Transform>().ComputeWorldTransform();
				Engine::Physics::contact_manifold manifold;
				std::pair<Entity, Entity> const entity_pair = std::pair(it1->first, it2->first);

				bool hull1_is_reference_face = false;
				EIntersectionType result = intersect_convex_hulls_sat(
					chi_1->m_data, tr_1, it1->first.ID(), 
					chi_2->m_data, tr_2, it2->first.ID(), 
					contact_stack_arr.data(), &contact_stack_size,
					&hull1_is_reference_face
				);

				// If an intersection is detected, store results in global contact data.
				if (result & EIntersectionType::eAnyIntersection)
				{
					// Record intersection
					m_data.m_intersection_results.emplace(entity_pair, result);
					m_data.m_entity_intersections[it1->first].emplace_back(it2->first);
					m_data.m_entity_intersections[it2->first].emplace_back(it1->first);
					
					contact_manifold new_cm;
					// Account for face-face intersection using different collider for reference face.
					new_cm.rigidbodies.first = hull1_is_reference_face ? it1->first : it2->first;
					new_cm.rigidbodies.second = hull1_is_reference_face ? it2->first : it1->first;

					if (new_cm.rigidbodies.first.IsValid() && new_cm.rigidbodies.second.IsValid())
					{
						new_cm.data.first_contact_index = mgr_global_contact_data.all_contacts.size();
						new_cm.data.contact_count = contact_stack_size;
						new_cm.data.is_edge_edge = (result == EIntersectionType::eEdgeIntersection);

						mgr_global_contact_data.all_contact_manifolds.push_back(new_cm);
						mgr_global_contact_data.all_contacts.insert(
							mgr_global_contact_data.all_contacts.end(),
							contact_stack_arr.begin(),
							contact_stack_arr.begin() + contact_stack_size
						);

#ifdef DEBUG_RENDER_CONTACTS

						auto const & all_contacts = mgr_global_contact_data.all_contacts;
						auto const & all_manifolds = mgr_global_contact_data.all_contact_manifolds;
						if (new_cm.data.is_edge_edge)
						{
							contact cm_contact = all_contacts[new_cm.data.first_contact_index];
							mgr_global_contact_data.debug_draw_lines.push_back(cm_contact.point);
							mgr_global_contact_data.debug_draw_lines.push_back(cm_contact.point + cm_contact.normal * cm_contact.penetration);
						}
						else
						{
							for (size_t i = new_cm.data.first_contact_index; i < new_cm.data.first_contact_index + new_cm.data.contact_count; i++)
							{
								contact cm_contact = all_contacts[i];
								mgr_global_contact_data.debug_draw_points.push_back(cm_contact.point);
							}
						}

#endif // RENDER_CONTACT_POINTS
					}

				}
				it2++;
			}
			it1++;
		}

	}
}