#include <gtest/gtest.h>
#include "..\..\engine\src\Engine\Managers\resource_manager_data.h"
#include <unordered_map>

using namespace Engine::Managers;

TEST(ResourceManager, RegisterType)
{
	resource_manager_data res_mgr_data;
	resource_type const mesh_type = res_mgr_data.register_type("Mesh", nullptr, nullptr);

	auto const& type_data = res_mgr_data.get_resource_type_data(mesh_type);
	EXPECT_STREQ(type_data.m_name.c_str(), "Mesh");
	EXPECT_EQ(type_data.m_loader, nullptr);
	EXPECT_EQ(type_data.m_unloader, nullptr);
	EXPECT_EQ(type_data.m_type_extensions.size(), 0);
	EXPECT_EQ(type_data.m_type_resources.size(), 0);
}

TEST(ResourceManager, RegisterTypes)
{
	resource_manager_data res_mgr_data;
	resource_type const mesh_type = res_mgr_data.register_type("Mesh", nullptr, nullptr);
	resource_type const texture_type = res_mgr_data.register_type("Texture", nullptr, nullptr);

	EXPECT_EQ(res_mgr_data.m_map_resource_type_to_collection.size(), 2);
}

TEST(ResourceManager, RegisterTypeExtension)
{
	resource_manager_data res_mgr_data;
	resource_type const texture_type = res_mgr_data.register_type("Texture", nullptr, nullptr);

	{
		res_mgr_data.register_type_extension(texture_type, ".png");
		auto const& type_data = res_mgr_data.get_resource_type_data(texture_type);
		EXPECT_TRUE(type_data.m_type_extensions.find(".png") != type_data.m_type_extensions.end());
		auto extension_types = res_mgr_data.get_extension_type(".png");
		EXPECT_TRUE(extension_types.find(texture_type) != extension_types.end());
	}

	{
		res_mgr_data.register_type_extension(texture_type, ".jpeg");
		auto const & type_data = res_mgr_data.get_resource_type_data(texture_type);
		EXPECT_TRUE(type_data.m_type_extensions.find(".png") != type_data.m_type_extensions.end());
		EXPECT_TRUE(type_data.m_type_extensions.find(".jpeg") != type_data.m_type_extensions.end());
		auto extension_types = res_mgr_data.get_extension_type(".jpeg");
		EXPECT_TRUE(extension_types.find(texture_type) != extension_types.end());
	}
}

TEST(ResourceManager, RegisterResource)
{
	resource_manager_data res_mgr_data;
	resource_type const texture_type = res_mgr_data.register_type("Texture", nullptr, nullptr);
	res_mgr_data.register_type_extension(texture_type, ".png");

	resource_id const tex1_id = res_mgr_data.register_resource(1, texture_type);
	EXPECT_EQ(res_mgr_data.get_resource_reference(tex1_id).m_resource_handle, 1);
	resource_id const tex2_id = res_mgr_data.register_resource(2, texture_type);
	EXPECT_EQ(res_mgr_data.get_resource_reference(tex1_id).m_resource_handle, 1);
	EXPECT_EQ(res_mgr_data.get_resource_reference(tex2_id).m_resource_handle, 2);
	EXPECT_TRUE(res_mgr_data.unload_resource(tex1_id));
	//EXPECT_EQ(res_mgr_data.get_resource_handle(tex1_id), 0);
	EXPECT_EQ(res_mgr_data.get_resource_reference(tex2_id).m_resource_handle, 2);
	EXPECT_FALSE(res_mgr_data.unload_resource(tex1_id));
	EXPECT_TRUE(res_mgr_data.unload_resource(tex2_id));
	EXPECT_FALSE(res_mgr_data.unload_resource(tex2_id));
}

struct path_hash
{
	std::size_t operator()(fs::path const& path) const {
		return fs::hash_value(path);
	}
};

TEST(ResourceManager, LoadResource)
{

	static std::unordered_map<fs::path, uint32_t, path_hash> loaded_paths;
	static std::unordered_map<uint32_t, fs::path> loaded_resources;
	static uint32_t handle_counter = 1;

	auto loader = [](fs::path const& _path)->uint32_t
	{
		uint32_t handle = 0;
		auto iter = loaded_paths.find(_path);
		if (iter == loaded_paths.end())
		{
			handle = handle_counter++;
			loaded_paths.emplace(_path, handle);
			loaded_resources.emplace(handle, _path);
		}
		return handle;
	};

	auto unloader = [](uint32_t _handle)
	{
		auto iter = loaded_resources.find(_handle);
		if (iter != loaded_resources.end())
		{
			loaded_paths.erase(iter->second);
			loaded_resources.erase(iter);
		}
	};

	resource_manager_data res_mgr_data;
	resource_type const texture_type = res_mgr_data.register_type("Texture", loader, unloader);
	res_mgr_data.register_type_extension(texture_type, ".png");
	res_mgr_data.register_type_extension(texture_type, ".jpeg");

	ASSERT_TRUE(loaded_paths.empty());
	ASSERT_TRUE(loaded_resources.empty());

	resource_id const example1_resource = res_mgr_data.load_resource("example1.png", texture_type);
	ASSERT_EQ(loaded_paths.size(), 1);
	auto it = loaded_paths.find("example1.png");
	auto it2 = loaded_resources.find(res_mgr_data.get_resource_reference(example1_resource).m_resource_id);
	ASSERT_TRUE(it != loaded_paths.end());
	ASSERT_TRUE(it2 != loaded_resources.end());

	resource_id const example2_resource = res_mgr_data.load_resource("example2.jpeg", texture_type);
	ASSERT_EQ(loaded_paths.size(), 2);

	res_mgr_data.unload_resource(example1_resource);
	it = loaded_paths.find("example1.png");
	it2 = loaded_resources.find(res_mgr_data.get_resource_reference(example1_resource).m_resource_id);
	ASSERT_FALSE(it != loaded_paths.end());
	ASSERT_FALSE(it2 != loaded_resources.end());
}