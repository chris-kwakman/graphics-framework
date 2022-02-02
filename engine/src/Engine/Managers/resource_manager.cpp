
#include "resource_manager.h"

#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>

#include <Engine/Utils/singleton.h>

namespace Engine {
namespace Managers {

void ResourceManager::DisplayEditorWidget()
{

	if (ImGui::Begin("Resources"))
	{
		static std::string path_filter = "";
		static resource_type type_filter = 0;
		static std::vector<std::pair<resource_id, fs::path>> precomputed_filtered_items;

		bool update_filters = m_new_resources;
		m_new_resources = false;
		if (ImGui::IsWindowAppearing())
		{
			path_filter = "";
			update_filters = true;
		}

		std::string type_filter_name;
		if (type_filter == 0)
			type_filter_name = "All Types";
		else
			type_filter_name = get_resource_type_data(type_filter).m_name;

		if (ImGui::BeginCombo("Type Filter", type_filter_name.c_str()))
		{
			if (type_filter != 0 && ImGui::MenuItem("Disable Filter"))
			{
				type_filter = 0;
				update_filters = true;
			}
			auto type_iter = m_map_resource_type_to_collection.begin();
			while (type_iter != m_map_resource_type_to_collection.end())
			{
				if (type_filter != type_iter->first && ImGui::MenuItem(type_iter->second.m_name.c_str()))
				{
					type_filter = type_iter->first;
					update_filters = true;
				}
				type_iter++;
			}
			ImGui::EndCombo();
		}

		if (ImGui::InputText("Name Filter", &path_filter))
		{
			update_filters = true;
		}

		if (update_filters)
		{
			update_filters = false;
			precomputed_filtered_items.clear();

			if (type_filter != 0)
			{
				auto const& type_data = get_resource_type_data(type_filter);
				auto resource_iter = type_data.m_type_resources.begin();
				while (resource_iter != type_data.m_type_resources.end())
				{
					decltype(precomputed_filtered_items)::value_type pair;
					pair.first = *resource_iter;
					pair.second = get_resource_path(*resource_iter);
					precomputed_filtered_items.emplace_back(std::move(pair));
					resource_iter++;
				}
			}
			else
			{
				auto resource_iter = m_map_resource_id_to_data.begin();
				while (resource_iter != m_map_resource_id_to_data.end())
				{
					decltype(precomputed_filtered_items)::value_type pair;
					pair.first = resource_iter->first;
					pair.second = resource_iter->second.m_path;
					precomputed_filtered_items.emplace_back(std::move(pair));
					++resource_iter;
				}
			}
		}

		if (ImGui::BeginTable("Resources", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable))
		{
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort);
			ImGui::TableSetupColumn("Type");
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < precomputed_filtered_items.size(); i++)
			{
				resource_reference const ref = Singleton<ResourceManager>().get_resource_reference(
					precomputed_filtered_items[i].first
				);
				resource_type const res_id_type = ref.m_resource_typeid.m_type;
				const char* type_name = get_resource_type_data(res_id_type).m_name.c_str();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(precomputed_filtered_items[i].second.string().c_str());
				resource_dragdrop_source(ref, type_name);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text(type_name);
			}
			ImGui::EndTable();
		}

	}
	ImGui::End();
}

void ResourceManager::TryDragDropFile(fs::path _path)
{
	if (!fs::exists(_path))
		return;

	auto path_extension_types = get_extension_type(_path.extension());
	if (path_extension_types.size() == 1)
	{
		resource_id new_id = load_resource(_path, *path_extension_types.begin());
		if (new_id != 0)
			m_new_resources = true;
	}
	else
	{
		m_drag_dropped_file_path = _path;
	}
}

void resource_dragdrop_source(resource_reference _reference, const char* _type_name)
{
	resource_type const named_type = Singleton<ResourceManager>().find_named_type(_type_name);
	if (named_type != 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload(_type_name, &_reference, sizeof(resource_reference));
		ImGui::Text(_type_name);
		ImGui::EndDragDropSource();
	}
}

void resource_dragdrop_target(resource_reference& _reference, const char * _type_name)
{
	resource_type const named_type = Singleton<ResourceManager>().find_named_type(_type_name);
	if (named_type != 0 && ImGui::BeginDragDropTarget())
	{
		if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload(_type_name))
		{
			assert(payload->DataSize == sizeof(resource_reference));
			_reference = *reinterpret_cast<resource_reference const*>(payload);
		}
		ImGui::EndDragDropTarget();
	}
}

}
}