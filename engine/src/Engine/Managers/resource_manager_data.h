#pragma once

#include <cstdint>
#include <engine/Utils/filesystem.h>
#include <unordered_map>
#include <set>

#include <nlohmann/json.hpp>

namespace Engine {
namespace Managers
{
	typedef uint32_t resource_id;
	typedef uint8_t	 resource_type;

	class ResourceManager;

	struct resource_typeid
	{
		resource_typeid() : m_type_and_id(0) {}
		resource_typeid(resource_id _id, resource_type _type) :
			m_id(_id),
			m_type(_type)
		{}

		union
		{
			uint32_t mutable m_type_and_id;
			struct
			{
				resource_id mutable m_id : 24;		// ID of resource in resource manager.
				resource_type mutable m_type : 8;	// Type of resource in resource manager.
			};
		};

		resource_typeid& operator=(resource_typeid const& _l);

		bool operator<(resource_typeid const& _l) const;
		bool operator==(resource_typeid const& _l) const;
		bool operator!=(resource_typeid const& _l) const;
	};

	void from_json(nlohmann::json const& _j, resource_typeid& _v);
	void to_json(nlohmann::json & _j, resource_typeid const & _v);
	
	typedef uint32_t(*fn_resource_loader)(fs::path const& _path);
	typedef void(*fn_resource_unloader)(uint32_t);

	struct resource_metadata
	{
		uint32_t		m_resource_handle = 0; // Generic resource handle.
		resource_type	m_type = 0;
		fs::path		m_path;
	};
	struct resource_type_data
	{
		resource_type_data(std::string _name, fn_resource_loader _loader, fn_resource_unloader _unloader) :
			m_name(_name),
			m_loader(_loader),
			m_unloader(_unloader)
		{}

		std::string const			m_name;
		std::set<fs::path>			m_type_extensions;
		std::vector<resource_id>	m_type_resources;

		fn_resource_loader const	m_loader;
		fn_resource_unloader const	m_unloader;
	};

	struct resource_manager_data
	{
		struct path_hash
		{
			std::size_t operator()(fs::path const& path) const {
				return fs::hash_value(path);
			}
		};

		friend struct resource_reference;

		// Serialized
		// A path can have multiple resources (i.e. .obj to convex hull and graphics mesh)
		std::unordered_map<fs::path, std::set<resource_typeid>, path_hash>	m_map_path_to_resource_id;
		std::unordered_map<fs::path, std::set<resource_type>, path_hash>	m_map_extension_to_resource_type;

		// Not serialized
		std::unordered_map<resource_id, resource_metadata>					m_map_resource_id_to_data;
		std::unordered_map<resource_type, resource_type_data>				m_map_resource_type_to_collection;

		resource_id		m_id_counter = 1;
		resource_type	m_type_counter = 1;

		void				reset();

		uint32_t 			get_resource_handle(resource_id const _id) const;
		fs::path const&		get_resource_path(resource_id const _id) const;
		resource_type		get_resource_type(resource_id const _id) const;
		std::set<resource_typeid> get_path_resources(fs::path const & _path) const;

		resource_id			load_resource(fs::path _path, resource_type _type);
		resource_id			register_resource(uint32_t const _handle, resource_type const _type, resource_id const _force_id = 0);
		bool				unload_resource(resource_id const _id);

		resource_type		register_type(std::string const _name, fn_resource_loader const _loader, fn_resource_unloader const _unloader);
		void				register_type_extension(resource_type const _type, fs::path const& _extension);
		std::set<resource_type> 		get_extension_type(fs::path const& _extension) const;

		resource_type_data& get_resource_type_data(resource_type _type);

		resource_type		find_named_type(const char* _name) const;

		bool				is_resource_registered(resource_id _id) const;

	protected:

		resource_id			load_resource_with_id(fs::path _path, resource_type _type, resource_id _id);

	private:

		resource_metadata* find_resource_data(resource_id _id);
		resource_metadata const* find_resource_data(resource_id _id) const;
		resource_id					get_new_id();
	};

}
}

namespace std
{
	template<>
	struct hash<Engine::Managers::resource_typeid> {
		inline size_t operator()(Engine::Managers::resource_typeid const& _x) {
			return std::hash<uint32_t>{}(_x.m_id);
		}
	};

}