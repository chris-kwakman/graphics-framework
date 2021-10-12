#include "LoadScene.h"

#include <Engine/Math/Transform3D.h>
#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Renderable.h>
#include <Engine/Components/Light.h>

#include <fstream>

namespace Sandbox
{
	using namespace nlohmann;
	using transform3D = Engine::Math::transform3D;
	using json = nlohmann::json;
	using Entity = Engine::ECS::Entity;

	json LoadJSON(const char* _path)
	{
		std::ifstream in_file(_path);
		// If we could not load path, return empty json
		if (!in_file.is_open())
			return json{};

		return json::parse(in_file);
	}

	void LoadScene(nlohmann::json const& _scene)
	{
		using namespace Component;
		using namespace Engine::ECS;

		unsigned int entity_count = 0;
		auto object_iter = _scene.find("objects");
		auto camera_iter = _scene.find("camera");
		auto light_iter = _scene.find("lights");
		if (object_iter != _scene.end()) entity_count += object_iter->size();
		if (camera_iter != _scene.end()) entity_count += 1;
		if (light_iter != _scene.end()) entity_count += light_iter->size();

		// Create entities up-front.
		std::vector<Entity> created_entities;
		created_entities.resize(entity_count);
		bool result = Singleton<EntityManager>().EntityCreationRequest(&created_entities.front(), entity_count);
		assert(result);
		unsigned int const lights_offset = object_iter->size();
		unsigned int const camera_offset = lights_offset + light_iter->size();
		unsigned int const node_count = lights_offset;

		for (auto entity : created_entities)
			Create<Transform>(entity);

		auto & resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		// Create root entities of scene
		for (unsigned int i = 0; i < lights_offset; ++i)
		{
			Entity current_entity = created_entities[i];
			auto current_transform = current_entity.GetComponent<Transform>();
			json const& object_json = object_iter->at(i);

			auto mesh_iter = object_json.find("mesh");
			if (mesh_iter != object_json.end())
			{
				current_transform.SetLocalTransform(object_json);
				std::string const model_path = mesh_iter->get<std::string>();
				if (!resource_manager.IsGLTFModelImported(model_path.c_str()))
					resource_manager.ImportModel_GLTF(model_path.c_str());
				json const gltf_json = LoadJSON(model_path.c_str());
				auto model_scene_entities = LoadGLTFScene(gltf_json, model_path.c_str());
				for (Entity model_scene_entity : model_scene_entities)
					model_scene_entity.GetComponent<Transform>().SetParent(current_entity);
			}
			current_entity.SetName(object_iter->at(i).at("mesh").get<std::string>().c_str());
		}

		for (unsigned int i = lights_offset; i < camera_offset; ++i)
		{
			Entity current_object = created_entities[i];
			json const& object_json = light_iter->at(i - lights_offset);
			auto light = Create<PointLight>(current_object);
			light.SetColor(object_json.at("color"));
			light.SetRadius(object_json.at("radius"));
			current_object.GetComponent<Transform>().SetLocalPosition(object_json.at("position"));
			char name_buffer[64];
			current_object.SetName("Light");
		}

		for (unsigned int i = camera_offset; i <= camera_offset; ++i)
		{
			Entity current_object = created_entities[i];
			json const& object_json = *camera_iter;
			current_object.GetComponent<Transform>().SetLocalTransform(object_json);
			auto camera = Create<Camera>(current_object);
			camera.SetNearDistance(object_json.at("near"));
			camera.SetFarDistance(object_json.at("far"));
			camera.SetVerticalFOV(object_json.at("FOVy") * glm::pi<float>() / 180.0f); // Convert to radians
		}
	}

	std::vector<Engine::ECS::Entity> LoadGLTFScene(nlohmann::json const& _scene, const char * _scene_path)
	{
		assert(_scene_path);

		using namespace Component;
		using namespace Engine::Graphics;

		// Early exit if there is no default scene since there will be no objects to create.
		auto iter_scene = _scene.find("scene");
		if (iter_scene == _scene.end())
			return {};
		json const& default_scene = *iter_scene;
		json const& scenes = _scene.at("scenes");
		json const& nodes = _scene.at("nodes");
		
		ResourceManager::gltf_model_data const& model_data = Singleton<ResourceManager>().GetImportedGLTFModelData(_scene_path);

		// Create all entities and their transforms in scene simultaneously
		std::vector<Entity> node_entities;
		node_entities.resize(_scene.at("nodes").size());
		Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&node_entities.front(), node_entities.size());
		for (Entity e : node_entities)
			Component::Create<Transform>(e);

		// First pass to check parent of each entity (if any)
		// Set up scene graph while we're at it.
		std::vector<int> node_parent_indices;
		node_parent_indices.resize(node_entities.size(), -1);
		for (unsigned int i = 0; i < nodes.size(); ++i)
		{
			json const& node = nodes.at(i);
			auto child_iter = node.find("children");
			if (child_iter != node.end())
			{
				for (unsigned int child_index : *child_iter)
				{
					node_parent_indices[child_index] = i;
					node_entities[child_index].GetComponent<Transform>().SetParent(node_entities[i]);
				}
			}
		}
		// Second pass to check for entities with no parents (i.e. the root nodes)
		std::vector<Entity> root_nodes;
		for (unsigned int i = 0; i < node_parent_indices.size(); ++i)
		{
			if (node_parent_indices[i] == -1)
				root_nodes.push_back(node_entities[i]);
		}

		for (unsigned int i = 0; i < node_entities.size(); ++i)
		{
			json const& node = nodes.at(i);
			Entity node_entity = node_entities[i];

			auto name_iter = node.find("name");
			auto mesh_iter = node.find("mesh");
			auto weights_iter = node.find("weights");

			// Set node name
			if (name_iter != node.end())
				node_entity.SetName(name_iter->get<std::string>().c_str());
			// Set renderable mesh
			if (mesh_iter != node.end())
			{
				auto renderable = Component::Create<Renderable>(node_entity);
				renderable.SetMesh(model_data.m_meshes[*mesh_iter]);
			}
			// Set weights
			if (weights_iter != node.end())
			{
				printf("[LoadGLTFScene] Weight parsing not handled yet.\n");
			}
		}


		return root_nodes;
	}

}
