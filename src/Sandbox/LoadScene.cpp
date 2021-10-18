#include "LoadScene.h"

#include <Engine/Math/Transform3D.h>
#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Renderable.h>
#include <Engine/Components/Light.h>

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
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

	void LoadScene(nlohmann::json const& _scene, const char * _scene_path)
	{
		using namespace Component;
		using namespace Engine::ECS;

		unsigned int entity_count = 0;
		auto object_iter = _scene.find("objects");
		auto camera_iter = _scene.find("camera");
		auto light_iter = _scene.find("lights");
		auto dirlight_iter = _scene.find("directional_light");
		if (object_iter != _scene.end()) entity_count += object_iter->size();
		if (camera_iter != _scene.end()) entity_count += 1;
		if (light_iter != _scene.end()) entity_count += light_iter->size();
		if (dirlight_iter != _scene.end()) entity_count += 1;

		// Create entities up-front.
		std::vector<Entity> created_entities;
		created_entities.resize(entity_count);
		bool result = Singleton<EntityManager>().EntityCreationRequest(&created_entities.front(), entity_count);
		assert(result);

		unsigned int lights_offset, dirlight_offset, camera_offset;

		lights_offset = object_iter->size();
		dirlight_offset = lights_offset + light_iter->size();
		camera_offset = (dirlight_iter != _scene.end()) ? dirlight_offset + 1 : dirlight_offset;
		unsigned int const node_count = object_iter->size();

		Singleton<SceneEntityComponentManager>().RegisterScene(_scene_path);

		for (auto entity : created_entities)
		{
			Component::Create<Transform>(entity);
			Component::Create<SceneEntityComponent>(entity).SetToScene(_scene_path);
		}

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
				auto model_scene_entities = LoadGLTFScene(gltf_json, model_path.c_str(), _scene_path);
				for (Entity model_scene_entity : model_scene_entities)
					model_scene_entity.GetComponent<Transform>().SetParent(current_entity, false);
			}
			current_entity.SetName(object_iter->at(i).at("mesh").get<std::string>().c_str());
		}

		for (unsigned int i = lights_offset; i < dirlight_offset; ++i)
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

		for (unsigned int i = dirlight_offset; i < camera_offset; ++i)
		{
			Entity current_object = created_entities[i];
			json const& object_json = *dirlight_iter;
			auto light = Create<DirectionalLight>(current_object);
			light.SetColor(object_json.at("color"));
			auto light_transform = current_object.GetComponent<Transform>();
			light_transform.SetLocalRotation(
				glm::quatLookAt(
					glm::vec3(object_json.at("direction")), 
					glm::vec3(0.0f, 1.0f, 0.0f)
				)
			);

			char name_buffer[64];
			current_object.SetName("Directional Light");
		}

		for (unsigned int i = camera_offset; i <= camera_offset; ++i)
		{
			Entity current_object = created_entities[i];
			current_object.SetName("Camera");
			json const& object_json = *camera_iter;
			current_object.GetComponent<Transform>().SetLocalTransform(object_json);
			auto camera = Create<Camera>(current_object);
			camera.SetNearDistance(object_json.at("near"));
			camera.SetFarDistance(object_json.at("far"));
			camera.SetVerticalFOV(object_json.at("FOVy") * glm::pi<float>() / 180.0f); // Convert to radians
		}
	}

	std::vector<Engine::ECS::Entity> LoadGLTFScene(nlohmann::json const& _scene, const char * _model_path, const char * _scene_path)
	{
		assert(_scene_path);
		assert(_model_path);

		using namespace Component;
		using namespace Engine::Graphics;

		// Early exit if there is no default scene since there will be no objects to create.
		auto iter_scene = _scene.find("scene");
		if (iter_scene == _scene.end())
			return {};
		json const& default_scene = *iter_scene;
		json const& scenes = _scene.at("scenes");
		json const& nodes = _scene.at("nodes");
		
		ResourceManager::gltf_model_data const& model_data = Singleton<ResourceManager>().GetImportedGLTFModelData(_model_path);

		// Create all entities and their transforms in scene simultaneously
		std::vector<Entity> node_entities;
		node_entities.resize(_scene.at("nodes").size());
		Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&node_entities.front(), node_entities.size());
		for (Entity e : node_entities)
		{
			Component::Create<Transform>(e);
			Component::Create<SceneEntityComponent>(e).SetToScene(_scene_path);
		}

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

namespace Component
{

	const char* SceneEntityComponentManager::GetComponentTypeName() const
	{
		return "SceneEntity";
	}

	void SceneEntityComponentManager::DestroySceneEntities(std::string const& _scene_path)
	{
		auto iter = m_scene_filepath_id.find(_scene_path);
		if (iter != m_scene_filepath_id.end()) destroy_scene_id_entities(iter->second);
	}

	void SceneEntityComponentManager::DestroyAllSceneEntities()
	{
		for (auto& pair : m_scene_filepath_id)
			destroy_scene_id_entities(pair.second);
	}

	void SceneEntityComponentManager::RegisterScene(std::string const& _scene_path)
	{
		auto scene_filepath_iter = m_scene_filepath_id.find(_scene_path);
		if (scene_filepath_iter == m_scene_filepath_id.end())
		{
			uint8_t const new_scene_id = m_scene_filepath_id.size();
			m_scene_filepath_id.emplace(_scene_path, new_scene_id);
			m_scene_id_entities.emplace(new_scene_id, std::vector<Engine::ECS::Entity>());
		}
	}

	void SceneEntityComponentManager::impl_clear()
	{
		m_scene_filepath_id.clear();
		m_entity_scene_id.clear();
		m_scene_id_entities.clear();
	}

	bool SceneEntityComponentManager::impl_create(Entity _e)
	{
		m_entity_scene_id.emplace(_e, INVALID_SCENE_ID);
		return true;
	}

	void SceneEntityComponentManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		std::vector<Entity> existing_entities;
		for (unsigned int i = 0; i < _count; ++i)
		{
			if (m_entity_scene_id.find(_entities[i]) != m_entity_scene_id.end())
				existing_entities.push_back(_entities[i]);
		}

		// After sorting entities by scene ID, remove from internal map
		for (Entity const destroy_entity : existing_entities)
			m_entity_scene_id.erase(destroy_entity);

		// Remove entities from respective scene id lists using entity list sorted by scene ID.
		unsigned int cached_scene_id = INVALID_SCENE_ID;
		decltype(m_scene_id_entities)::mapped_type * scene_id_entities_ptr;
		
		for (auto& scene_map : m_scene_id_entities)
		{
			auto scene_entity_iter = scene_map.second.begin();
			while(scene_entity_iter != scene_map.second.end())
			{
				auto iter = existing_entities.begin();
				while (iter != existing_entities.end())
				{
					if (*scene_entity_iter == *iter)
					{
						scene_entity_iter = scene_map.second.erase(scene_entity_iter);
						existing_entities.erase(iter);
						goto next_iter;
					}
					iter++;
				}
				scene_entity_iter++;
			next_iter:;
			}
		}
	}

	bool SceneEntityComponentManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return (m_entity_scene_id.find(_entity) != m_entity_scene_id.end());
	}

	void SceneEntityComponentManager::impl_edit_component(Entity _entity)
	{
		uint8_t const entity_scene_id = m_entity_scene_id.at(_entity);
		std::string scene_filepath = "None";
		for (auto pair : m_scene_filepath_id)
		{
			if (pair.second == entity_scene_id)
			{
				scene_filepath = pair.first;
				break;
			}
		}
		ImGui::Text("Scene: %s", scene_filepath.c_str());
	}

	void SceneEntityComponentManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	void SceneEntityComponentManager::destroy_scene_id_entities(uint8_t _id)
	{
		auto& scene_entity_list = m_scene_id_entities.at(_id);

		Singleton<Engine::ECS::EntityManager>().EntityDelayedDeletion(
			&scene_entity_list.front(),
			scene_entity_list.size()
		);
		scene_entity_list.clear();
	}

	void SceneEntityComponentManager::set_entity_scene_id(Entity _e, uint8_t _id)
	{
		uint8_t const current_scene_id = m_entity_scene_id.at(_e);
		if (current_scene_id == _id)
			return;

		// Remove from current scene (if any)
		if (current_scene_id != INVALID_SCENE_ID)
		{
			auto& scene_id_entities = m_scene_id_entities.at(current_scene_id);
			std::remove(scene_id_entities.begin(), scene_id_entities.end(), _e);
		}
		// Add to new scene (if valid one is set)
		if (_id != INVALID_SCENE_ID);
		{
			auto& scene_id_entities = m_scene_id_entities.at(_id);
			scene_id_entities.push_back(_e);
		}
		m_entity_scene_id.at(_e) = _id;
	}

	void SceneEntityComponent::SetToScene(std::string const& _scene_path)
	{
		auto iter = GetManager().m_scene_filepath_id.find(_scene_path);
		if (iter != GetManager().m_scene_filepath_id.end())
			GetManager().set_entity_scene_id(m_owner, iter->second);
		else
			GetManager().set_entity_scene_id(m_owner, SceneEntityComponentManager::INVALID_SCENE_ID);
	}

}