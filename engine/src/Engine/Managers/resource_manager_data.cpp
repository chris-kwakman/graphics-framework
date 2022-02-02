#include "resource_manager.h"
#include <algorithm>
#include <cassert>
#include <filesystem>

namespace Engine {
namespace Managers {

	void resource_manager_data::reset()
	{
		// Unload all resources
		while (!m_map_resource_id_to_data.empty())
		{
			auto resource_iter = m_map_resource_id_to_data.begin();
			unload_resource(resource_iter->first);
		}

		// Hard-reset on data.

		m_map_path_to_resource_id.clear();
		m_map_extension_to_resource_type.clear();
		m_map_resource_id_to_data.clear();
		m_map_resource_type_to_collection.clear();

		m_id_counter = 1;
		m_type_counter = 1;
	}

	resource_id resource_manager_data::load_resource(fs::path _path, resource_type _type)
	{
		// Get path of resource relative to executable's working directory.
		_path = std::filesystem::relative(std::filesystem::path(_path), std::filesystem::current_path());

		// Check if type actually exists
		auto type_iter = m_map_resource_type_to_collection.find(_type);
		if (type_iter == m_map_resource_type_to_collection.end())
			return 0;

		// Check if path extension matches one of the extensions covered by input type.
		auto const& extensions = type_iter->second.m_type_extensions;
		if (auto extension_iter = extensions.find(_path.extension()); extension_iter == extensions.end())
			return 0;

		// Check if path has already been loaded for the given type.
		auto resource_path_iter = m_map_path_to_resource_id.find(_path);
		if (resource_path_iter != m_map_path_to_resource_id.end())
		{
			// Check if resources corresponding to path are of type input type.
			for (resource_typeid const path_resource_typeid : resource_path_iter->second)
			{
				if (get_resource_type(path_resource_typeid.m_id) == _type)
					return path_resource_typeid.m_id;
			}
		}

		uint32_t const resource_handle = type_iter->second.m_loader(_path);
		if (resource_handle == 0)
			return 0;

		resource_id const new_resource_id = register_resource(resource_handle, _type);
		m_map_path_to_resource_id[_path].emplace(new_resource_id, _type);
		m_map_resource_id_to_data.at(new_resource_id).m_path = _path;
		return new_resource_id;
	}

	resource_id resource_manager_data::register_resource(uint32_t const _handle, resource_type const _type)
	{
		if (_type == 0 || _handle == 0)
			return 0;

		resource_metadata res_md;
		res_md.m_resource_handle = _handle;
		res_md.m_path = "";
		res_md.m_type = _type;

		resource_id const new_id = get_new_id();
		m_map_resource_id_to_data.emplace(new_id, res_md);
		auto & type_data = get_resource_type_data(_type);
		type_data.m_type_resources.insert(
			std::upper_bound(
				type_data.m_type_resources.begin(),
				type_data.m_type_resources.end(),
				new_id
			),
			new_id
		);
		return new_id;
	}

	bool resource_manager_data::unload_resource(resource_id const _id)
	{
		if (_id == 0)
			return false;
		auto resource_iter = m_map_resource_id_to_data.find(_id);
		if(resource_iter == m_map_resource_id_to_data.end())
			return false;

		auto& type_data = m_map_resource_type_to_collection.at(resource_iter->second.m_type);

		if(type_data.m_unloader)
			type_data.m_unloader(resource_iter->second.m_resource_handle);

		if (!resource_iter->second.m_path.empty())
			m_map_path_to_resource_id.erase(resource_iter->second.m_path);

		auto type_resource_list_iter = std::lower_bound(type_data.m_type_resources.begin(), type_data.m_type_resources.end(), _id);
		type_data.m_type_resources.erase(type_resource_list_iter);
		m_map_resource_id_to_data.erase(resource_iter);
		return true;
	}

	resource_reference resource_manager_data::get_resource_reference(resource_id const _id) const
	{
		resource_metadata const* md = find_resource_data(_id);
		if (md == nullptr)
			return resource_reference(0, 0, 0);
		
		return resource_reference(_id, md->m_type, md->m_resource_handle);
	}

	fs::path const& resource_manager_data::get_resource_path(resource_id const _id) const
	{
		assert(_id != 0 && "NULL resource ID");
		resource_metadata const* data = find_resource_data(_id);
		assert(data && "Invalid resource ID");
		return data->m_path;
	}

	resource_type resource_manager_data::get_resource_type(resource_id const _id) const
	{
		auto resource_iter = m_map_resource_id_to_data.find(_id);
		if (resource_iter == m_map_resource_id_to_data.end())
			return 0;
		else
			return resource_iter->second.m_type;
	}

	resource_type resource_manager_data::find_named_type(const char* _name) const
	{
		assert(_name != nullptr);
		auto type_iter = m_map_resource_type_to_collection.begin();
		while (type_iter != m_map_resource_type_to_collection.end())
		{
			if (strcmp(type_iter->second.m_name.c_str(), _name) == 0)
				return type_iter->first;
			++type_iter;
		}
		return 0;
	}

	resource_type resource_manager_data::register_type(std::string const _name, fn_resource_loader const _loader, fn_resource_unloader const _unloader)
	{
		// Check if any type with input name already exists.
		for (auto iter : m_map_resource_type_to_collection)
		{
			auto const& data = iter.second;
			assert(data.m_name != _name);
		}

		resource_type const new_type_id = m_type_counter++;
		resource_type_data const type_data(_name, _loader, _unloader);
		m_map_resource_type_to_collection.emplace(new_type_id, type_data);
		return new_type_id;
	}

	void resource_manager_data::register_type_extension(resource_type const _type, fs::path const& _extension)
	{
		auto & data = get_resource_type_data(_type);

		// Break out early if extension is already registered.
		for (auto const & ext : data.m_type_extensions)
		{
			if (ext == _extension)
				return;
		}
		data.m_type_extensions.emplace(_extension);
		m_map_extension_to_resource_type[_extension].emplace(_type);
	}

	std::set<resource_type> resource_manager_data::get_extension_type(fs::path const& _extension) const
	{
		auto ext_iter = m_map_extension_to_resource_type.find(_extension);
		if (ext_iter == m_map_extension_to_resource_type.end())
			return {};
		else
			return ext_iter->second;
	}

	resource_metadata * resource_manager_data::find_resource_data(resource_id const _id)
	{
		auto it = m_map_resource_id_to_data.find(_id);
		if (it != m_map_resource_id_to_data.end())
			return &it->second;
		else
			return nullptr;
	}
	
	resource_metadata const* resource_manager_data::find_resource_data(resource_id const _id) const
	{
		if (auto it = m_map_resource_id_to_data.find(_id); it != m_map_resource_id_to_data.end())
			return &it->second;
		else
			return nullptr;
	}

	resource_type_data& resource_manager_data::get_resource_type_data(resource_type _type)
	{
		return m_map_resource_type_to_collection.at(_type);
	}


	resource_id resource_manager_data::get_new_id()
	{
		resource_id id_iterator = m_id_counter-1;
		decltype(m_map_resource_id_to_data)::iterator iter;
		do
		{
			id_iterator++;
			iter = m_map_resource_id_to_data.find(id_iterator);
		} while (iter != m_map_resource_id_to_data.end());
		return id_iterator;
	}

	bool resource_typeid::operator<(resource_typeid const& _l) const
	{
		if (m_type == _l.m_type)
			return m_id < _l.m_id;
		return m_type < _l.m_type;
	}

	resource_typeid& resource_typeid::operator=(resource_typeid const& _l)
	{
		assert(m_type == _l.m_type);
		m_id = _l.m_id;
		return *this;
	}

	bool resource_typeid::operator==(resource_typeid const& _l) const
	{
		return m_type_and_id == _l.m_type_and_id;
	}

	bool resource_typeid::operator!=(resource_typeid const& _l) const
	{
		return m_type_and_id != _l.m_type_and_id;
	}

	bool resource_reference::operator==(resource_reference const& _l) const
	{
		return (m_resource_typeid == _l.m_resource_typeid) && (m_resource_handle == _l.m_resource_handle);
	}

	bool resource_reference::operator!=(resource_reference const& _l) const
	{
		return m_resource_typeid == _l.m_resource_typeid;
	}

	resource_reference& resource_reference::operator=(resource_reference const& _l)
	{
		assert(m_resource_typeid.m_type == _l.m_resource_typeid.m_type);  
		m_resource_typeid = _l.m_resource_typeid;
		m_resource_handle = _l.m_resource_handle; 
		return *this;
	}

}
}