
#include "resource_manager.h"

#include <Engine/Editor/editor.h>
#include <Engine/Utils/singleton.h>

namespace Engine {
namespace Managers {

	Resource::Resource(resource_typeid _type) :
		resource_typeid(_type)
	{}

	Resource::Resource(Resource const& _l) :
		resource_typeid(_l.m_id, _l.m_type)
	{}

	bool Resource::operator==(Resource const& _l) const
	{
		return *this == _l;
	}

	bool Resource::operator!=(Resource const& _l) const
	{
		return *this != _l;
	}

	bool Resource::operator<(Resource const& _l) const
	{
		return *this < _l;
	}

	uint32_t Resource::Handle() const
	{
		if (ID() == 0 || Type() == 0)
			return 0;
		auto const & resource_mgr = Singleton<Engine::Managers::ResourceManager>();
		return resource_mgr.get_resource_handle(m_id);
	}

	std::string Resource::Name() const
	{
		if (ID() == 0)
			return "Null";
		else if (Singleton<Engine::Managers::ResourceManager>().is_resource_registered(ID()))
			return Singleton<Engine::Managers::ResourceManager>().get_resource_path(ID()).string();
		else
			return "Null";
	}

	void ResourceManager::ImportSceneResources(nlohmann::json const& _scene_resources)
	{
		int const serializer_version = _scene_resources["serializer_version"];

		if (serializer_version == 1)
		{
			std::vector<std::pair<std::string, std::set<resource_typeid>>> path_resource_typeid = _scene_resources["path_typeids"];
			std::vector<std::pair<resource_type, std::string>> named_types = _scene_resources["type_names"];

			std::unordered_map<resource_type, resource_type> remap_to_internal_type_value;
			for (auto const & pair : named_types)
				remap_to_internal_type_value.emplace(pair.first, pair.first);

			// Compare against currently registered type names.
			// If loaded type value of loaded type name does not match internal type name's value,
			// re-map loaded type value to internal type value.
			for (auto loaded_type_name : named_types)
			{
				std::string const& type_name = loaded_type_name.second;
				for (auto const & internal_type : m_map_resource_type_to_collection)
				{
					if (internal_type.second.m_name == type_name)
						remap_to_internal_type_value[loaded_type_name.first] = internal_type.first;
				}
			}

			// Remap resource ID types and named types.
			for (auto& pair : path_resource_typeid)
			{
				for (auto & path_typeid : pair.second)
				{
					path_typeid.m_type = remap_to_internal_type_value.at(path_typeid.m_type);
				}
			}
			for (auto& pair : named_types)
				pair.first = remap_to_internal_type_value.at(pair.first);

			// Load resources one by one.
			for (auto const & pair : path_resource_typeid)
			{
				auto const & [path, path_resources] = pair;
				for (auto resource : path_resources)
				{
					resource_id const result_id = load_resource_with_id(fs::path(path), resource.m_type, resource.m_id);
					assert(result_id == resource.m_id && "Serialized resource ID and internal resource ID do not match.");
				}
			}
		}
	}

	void ResourceManager::ExportSceneResources(nlohmann::json& _scene_resources)
	{
		_scene_resources["serializer_version"] = 1;

		std::vector<std::pair<std::string, std::set<resource_typeid>>> path_resource_typeids;
		std::vector<std::pair<resource_type, std::string>> type_name_list;

		for (auto pair : m_map_path_to_resource_id)
			path_resource_typeids.emplace_back(std::pair(pair.first.string(), pair.second));
		_scene_resources["path_typeids"] = path_resource_typeids;

		for (auto const & pair : m_map_resource_type_to_collection)
			type_name_list.emplace_back(pair.first, pair.second.m_name);
		_scene_resources["type_names"] = type_name_list;
	}

	void ResourceManager::DisplayEditorWidget()
{
	auto const & resource_manager = Singleton<ResourceManager>();

	if (ImGui::Begin("Resources"))
	{
		static std::string path_filter = "";
		static resource_type type_filter = 0;
		static std::vector<std::pair<resource_id, fs::path>> cached_items;

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
			cached_items.clear();

			if (type_filter != 0)
			{
				auto const& type_data = get_resource_type_data(type_filter);
				auto resource_iter = type_data.m_type_resources.begin();
				while (resource_iter != type_data.m_type_resources.end())
				{
					decltype(cached_items)::value_type pair;
					pair.first = *resource_iter;
					pair.second = get_resource_path(*resource_iter);
					cached_items.emplace_back(std::move(pair));
					resource_iter++;
				}
			}
			else
			{
				auto resource_iter = m_map_resource_id_to_data.begin();
				while (resource_iter != m_map_resource_id_to_data.end())
				{
					decltype(cached_items)::value_type pair;
					pair.first = resource_iter->first;
					pair.second = resource_iter->second.m_path;
					cached_items.emplace_back(std::move(pair));
					++resource_iter;
				}
			}
		}

		if (ImGui::BeginTable("Resources", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable))
		{
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_DefaultSort);
			ImGui::TableSetupColumn("Type");
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < cached_items.size(); i++)
			{
				resource_type const res_id_type = resource_manager.get_resource_type(
					cached_items[i].first
				);
				const char* type_name = get_resource_type_data(res_id_type).m_name.c_str();

				Resource res = Resource(resource_typeid(cached_items[i].first, res_id_type));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(cached_items[i].second.string().c_str());
				resource_dragdrop_source(res, type_name);
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

Resource ResourceManager::GetResourceFromID(resource_id _id) const
{
	if (is_resource_registered(_id))
	{
		resource_typeid res_typeid;
		res_typeid.m_id = _id;
		res_typeid.m_type = get_resource_type(_id);
		return Resource(res_typeid);
	}
	return Resource();
}

void from_json(nlohmann::json const& _j, Resource& _v)
{
	from_json(_j, static_cast<resource_typeid &>(_v));
}

void to_json(nlohmann::json& _j, Resource const& _v)
{
	to_json(_j, static_cast<resource_typeid const&>(_v));
}

void resource_dragdrop_source(Resource const _resource, const char* _type_name)
{
	resource_type const named_type = Singleton<ResourceManager>().find_named_type(_type_name);
	if (named_type != 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload(_type_name, &_resource, sizeof(Resource));
		ImGui::Text(_type_name);
		ImGui::EndDragDropSource();
	}
}

bool resource_dragdrop_target(Resource & _resource, const char * _type_name)
{
	bool success = false;
	std::string resource_name = _resource.Name();
	ImGui::InputText("Resource", &resource_name, ImGuiInputTextFlags_ReadOnly);
	resource_type const named_type = Singleton<ResourceManager>().find_named_type(_type_name);
	if (named_type != 0 && ImGui::BeginDragDropTarget())
	{
		if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload(_type_name))
		{
			assert(payload->DataSize == sizeof(Resource));
			Resource const source_resource = *(Resource *)payload->Data;
			_resource = source_resource;
			success = true;
		}
		ImGui::EndDragDropTarget();
	}
	return success;
}

}
}