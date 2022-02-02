
#include "resource_manager.h"

#include <ImGui/imgui.h>

namespace Engine {
namespace Managers {

void ResourceManager::DisplayEditorWidget()
{
	static std::string path_filter = "";
	static resource_type type_filter = 0;
	static std::vector<std::pair<resource_id, fs::path>> precomputed_filtered_items;

	bool update_filters = false;
	if (ImGui::IsWindowAppearing())
		update_filters = true;

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

	char buffer[64];
	if (ImGui::InputText("Name Filter", buffer, sizeof(buffer)))
	{
		path_filter = buffer;
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
	}

	if (ImGui::BeginTable("Resources", 2, ImGuiTableFlags_Borders))
	{
		ImGui::NextColumn();
		ImGui::TableSetupColumn("Path");
		for (size_t i = 0; i < precomputed_filtered_items.size(); i++)
		{
			ImGui::TableNextRow();
			ImGui::Text(precomputed_filtered_items[i].second.string().c_str());
		}
		ImGui::NextColumn();
		ImGui::TableSetupColumn("Type");
		for (size_t i = 0; i < precomputed_filtered_items.size(); i++)
		{
			resource_type const res_id_type = get_resource_type(precomputed_filtered_items[i].first);
			ImGui::TableNextRow();
			ImGui::Text(get_resource_type_data(res_id_type).m_name.c_str());
		}
	}
	ImGui::EndTable();
}

}
}