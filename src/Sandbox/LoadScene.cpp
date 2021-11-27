#include "LoadScene.h"

#include <Engine/Math/Transform3D.h>
#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Renderable.h>
#include <Engine/Components/Light.h>
#include <Engine/Components/SkeletonAnimator.h>
#include <Engine/Components/CurveInterpolator.h>
#include <Engine/Components/CurveFollower.h>

#include "Components/PlayerController.h"

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

	Entity find_first_entity_with_animator_comp(Entity _e)
	{
		auto transform = _e.GetComponent<Component::Transform>();
		auto children = transform.GetChildren();
		for (auto child : children)
		{
			if (child.HasComponent<Component::SkeletonAnimator>())
				return child;
		}
		for (auto child : children)
		{
			Entity value = find_first_entity_with_animator_comp(child);
			if (value != Entity::InvalidEntity)
				return value;
		}
		return Entity::InvalidEntity;
	}

	void LoadScene(nlohmann::json const& _scene, const char* _scene_path)
	{
		using namespace Component;
		using namespace Engine::ECS;

		int decal_count = 0;
		int dirlight_count = 0;
		int pointlight_count = 0;
		int camera_count = 0;
		int object_count = 0;

		unsigned int entity_count = 0;
		auto object_iter = _scene.find("objects");
		auto camera_iter = _scene.find("camera");
		auto light_iter = _scene.find("lights");
		auto dirlight_iter = _scene.find("directional_light");
		auto decal_iter = _scene.find("decals");
		if (object_iter != _scene.end()) {
			object_count += (int)object_iter->size();
		}
		if (camera_iter != _scene.end()) {
			camera_count += 1;
		}
		if (light_iter != _scene.end()) {
			pointlight_count += (int)light_iter->size();
		}
		if (dirlight_iter != _scene.end()) {
			dirlight_count += 1;
		}
		if (decal_iter != _scene.end()) {
			entity_count += decal_count;
		}

		entity_count += object_count;
		entity_count += decal_count;
		entity_count += dirlight_count;
		entity_count += pointlight_count;
		entity_count += camera_count;

		// Create entities up-front.
		std::vector<Entity> created_entities;
		created_entities.resize(entity_count);
		bool result = Singleton<EntityManager>().EntityCreationRequest(&created_entities.front(), entity_count);
		assert(result);

		unsigned int lights_offset, dirlight_offset, camera_offset, decal_offset;

		lights_offset = (unsigned int)object_iter->size();
		dirlight_offset = lights_offset + (unsigned int)light_iter->size();
		camera_offset = (dirlight_iter != _scene.end()) ? dirlight_offset + 1 : dirlight_offset;
		decal_offset = (camera_iter != _scene.end()) ? camera_offset + 1 : camera_offset;
		unsigned int const node_count = (unsigned int)object_iter->size();

		Singleton<SceneEntityComponentManager>().RegisterScene(_scene_path);

		for (auto entity : created_entities)
		{
			Component::Create<Transform>(entity);
			Component::Create<SceneEntityComponent>(entity).SetToScene(_scene_path);
		}

		auto & resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		// Create root entities of scene
		for (unsigned int i = 0; i < (unsigned int)object_count; ++i)
		{
			Entity current_entity = created_entities[i];
			auto current_transform = current_entity.GetComponent<Transform>();
			json const& object_json = object_iter->at(i);
			current_transform.SetLocalTransform(object_json);

			auto name_iter = object_json.find("name");
			auto mesh_iter = object_json.find("mesh");
			auto animator_iter = object_json.find("skeleton_animator");
			auto curve_interp_iter = object_json.find("curve_interpolator");
			auto curve_follower_iter = object_json.find("curve_follower");
			if (mesh_iter != object_json.end())
			{
				std::string const model_path = mesh_iter->get<std::string>();
				if (!resource_manager.IsGLTFModelImported(model_path.c_str()))
					resource_manager.ImportModel_GLTF(model_path.c_str());
				json const gltf_json = LoadJSON(model_path.c_str());
				auto model_scene_entities = LoadGLTFScene(gltf_json, model_path.c_str(), _scene_path);
				for (Entity model_scene_entity : model_scene_entities)
					model_scene_entity.GetComponent<Transform>().SetParent(current_entity, false);
				current_entity.SetName(object_iter->at(i).at("mesh").get<std::string>().c_str());
			}
			if (curve_interp_iter != object_json.end())
			{
				auto curve = Component::Create<CurveInterpolator>(current_entity);
				piecewise_curve	new_pw_curve;

				auto adaptive_state_iter = curve_interp_iter->find("adaptive");

				std::string curve_type_name = curve_interp_iter->at("type");
				piecewise_curve::EType new_curve_type = piecewise_curve::EType::Linear;
				if (curve_type_name == "catmull")
					new_curve_type = piecewise_curve::EType::Catmull;
				else if (curve_type_name == "hermite")
					new_curve_type = piecewise_curve::EType::Hermite;
				else if (curve_type_name == "bezier")
					new_curve_type = piecewise_curve::EType::Bezier;
				else
					new_curve_type = piecewise_curve::EType::Linear;
				new_pw_curve.set_curve_type(new_curve_type);

				new_pw_curve.m_nodes = curve_interp_iter->at("nodes").get<std::vector<glm::vec3>>();

				if (adaptive_state_iter == curve_interp_iter->end()|| !adaptive_state_iter->get<bool>())
					curve.SetPiecewiseCurve(new_pw_curve, curve_interp_iter->at("resolution"));
				else
					curve.SetPiecewiseCurve(new_pw_curve, curve_interp_iter->at("tolerance"), curve_interp_iter->at("max_subdivisions"));
			}
			if (curve_follower_iter != object_json.end())
			{
				auto follower = Component::Create<CurveFollower>(current_entity);
				auto & data = follower.GetFollowerData();
				
				data.m_arclength = curve_follower_iter->value("arclength", 0.0f);
				data.m_max_travel_rate = curve_follower_iter->value("max_travel_rate", 10.0f);
				data.m_loop = curve_follower_iter->value("loop", true);
				data.set_linear_dist_time_func_data(1.0f);

				auto object_index_iter = curve_follower_iter->find("curve_object_index");
				if (object_index_iter != curve_follower_iter->end())
					data.m_curve_component = created_entities[object_index_iter->get<unsigned int>()].GetComponent<CurveInterpolator>();

				follower.SetPlayingState(curve_follower_iter->value("playing", false));

				std::string const distance_time_func_name = curve_follower_iter->value("distance_time_func", "linear");

				if (distance_time_func_name == "linear")
				{
					data.set_linear_dist_time_func_data(curve_follower_iter->value("travel_rate", 1.0f));
				}
				else if (distance_time_func_name == "sine_interpolation")
				{
					float param_travel_rate = curve_follower_iter->value("travel_rate", 0.1f);
					float seg_front_param = curve_follower_iter->value("seg_back_param", 0.25f);
					float seg_back_param = curve_follower_iter->value("seg_front_param", 0.25f);;
					data.set_sine_interp_dist_time_func_data(
						param_travel_rate, 
						seg_front_param, 
						seg_back_param
					);
				}
			}
			Component::SkeletonAnimator skeleton_animator = Entity::InvalidEntity;
			if (animator_iter != object_json.end())
			{
				Entity animator_entity = find_first_entity_with_animator_comp(current_entity);
				if (animator_entity != Entity::InvalidEntity)
				{
					skeleton_animator = animator_entity.GetComponent<Component::SkeletonAnimator>();
					// Due to how GLTF object is loaded, animator is guaranteed to have Skin Component.
					auto skin_joints = animator_entity.GetComponent<Component::Skin>().GetSkeletonInstanceNodes();
					animation_pose new_pose;
					new_pose.m_joint_transforms.resize(skin_joints.size());
					for (unsigned int i = 0; i < skin_joints.size(); ++i)
						new_pose.m_joint_transforms[i] = skin_joints[i].GetLocalTransform();

					skeleton_animator.Deserialize(*animator_iter);
					skeleton_animator.SetBindPose(std::move(new_pose));

					auto blendtree_file = animator_iter->find("blendtree_file");
					if (blendtree_file != animator_iter->end())
					{
						skeleton_animator.LoadBlendTree(blendtree_file->get<std::string>());
					}
				}
			}
			if (name_iter != object_json.end())
				current_entity.SetName(name_iter->get<std::string>().c_str());

			auto player_controller_iter = object_json.find("has_player_controller");
			if (player_controller_iter != object_json.end())
			{
				auto player_controller = Component::Create<Component::PlayerController>(current_entity);
				if (skeleton_animator.IsValid())
				{
					auto & root = skeleton_animator.GetBlendTreeRootNode();
					animation_tree_node * root_ptr = root.get();
					animation_blend_1D* cast_root_ptr = dynamic_cast<animation_blend_1D*>(root_ptr);
					if (cast_root_ptr)
					{
						animation_blend_1D* move_ptr = dynamic_cast<animation_blend_1D *>(
							cast_root_ptr->m_child_blend_nodes[0].get()
						);
						player_controller.BindMoveBlendParameter(&move_ptr->m_blend_parameter);
						animation_blend_2D* look_ptr = dynamic_cast<animation_blend_2D*>(
							cast_root_ptr->m_child_blend_nodes[1].get()
						);
						player_controller.BindLookBlendParameter(&look_ptr->m_blend_parameter);
					}
				}
			}
		}

		for (unsigned int i = lights_offset; i < lights_offset + pointlight_count; ++i)
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

		for (unsigned int i = dirlight_offset; i < dirlight_offset + dirlight_count; ++i)
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

		for (unsigned int i = camera_offset; i < camera_offset + camera_count; ++i)
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

		for (unsigned int i = decal_offset; i < decal_offset + decal_count; ++i)
		{
			Entity current_object = created_entities[i];
			current_object.SetName("Decal");
			json const& decal_json = decal_iter->at(i - decal_offset);

			std::vector<filepath_string> decal_texture_filepaths{
				decal_json.at("diffuse").get<std::string>(),
				decal_json.at("metallic").get<std::string>(),
				decal_json.at("normal").get<std::string>()
			};

			auto loaded_texture_handles = Singleton<ResourceManager>().LoadTextures(decal_texture_filepaths);

			Decal new_decal = Component::Create<Decal>(current_object);
			decal_textures & obj_decal_textures = new_decal.GetTexturesRef();
			
			obj_decal_textures.m_texture_albedo = loaded_texture_handles[0];
			obj_decal_textures.m_texture_metallic_roughness = loaded_texture_handles[1];
			obj_decal_textures.m_texture_normal = loaded_texture_handles[2];

			Transform obj_transform = current_object.GetComponent<Transform>();
			obj_transform.SetLocalTransform(decal_json);
		}
	}

	std::vector<Engine::ECS::Entity> LoadGLTFScene(nlohmann::json const& _scene, const char * _model_path, const char * _scene_path)
	{
		assert(_model_path);
		auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

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
		Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&node_entities.front(), (unsigned int)node_entities.size());
		for (Entity e : node_entities)
		{
			Component::Create<Transform>(e);
			if(_scene_path)
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
			node_entities[i].GetComponent<Transform>().SetLocalTransform(node);
			if (child_iter != node.end())
			{
				for (unsigned int child_index : *child_iter)
				{
					node_parent_indices[child_index] = i;
					node_entities[child_index].GetComponent<Transform>().SetParent(node_entities[i], false);
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
				// Set skin
				auto skin_iter = node.find("skin");
				if (skin_iter != node.end())
				{
					json const& json_skins = _scene.at("skins");
					json const & json_skin_node = json_skins.at(skin_iter->get<unsigned int>());
					json const& json_joints = json_skin_node.at("joints");
					std::vector<Transform> skin_joints;
					skin_joints.reserve(json_joints.size());
					for (unsigned int skin_joint : json_joints)
						skin_joints.emplace_back(node_entities[skin_joint]);

					Entity const skeleton_root_entity = node_entities[
						json_skin_node.value("skeleton", json_joints.front())
					];

					Component::Skin skin_component = Component::Create<Skin>(node_entity);
					skin_component.SetSkin(model_data.m_skins[*skin_iter]);
					skin_component.SetSkeletonInstanceNodes(skin_joints);
					skin_component.SetSkeletonRootNode(skeleton_root_entity);
					skin_component.SetShouldRenderJoints(false);

					if (!skeleton_root_entity.HasComponent<Component::Skin>())
					{
						Component::Skin root_skin_component = Component::Create<Skin>(skeleton_root_entity);
						root_skin_component.SetSkin(model_data.m_skins[*skin_iter]);
						root_skin_component.SetSkeletonInstanceNodes(skin_joints);
						root_skin_component.SetSkeletonRootNode(skeleton_root_entity);
						root_skin_component.SetShouldRenderJoints(false);
					}

					// Add skeleton animator component only to skeleton root node.
					auto animations_iter = _scene.find("animations");
					if (animations_iter != _scene.end() && !skeleton_root_entity.HasComponent<Component::SkeletonAnimator>())
					{
						char int_buffer[3];
						std::string animation_name;
						json const& anim_0 = animations_iter->at(0);
						auto anim_name_iter = anim_0.find("name");
						animation_name = std::filesystem::path(_model_path).stem().string() + "/"
							+ (anim_name_iter == anim_0.end() 
								? _itoa(0, int_buffer, 10)
								: anim_name_iter->get<std::string>());

						Component::SkeletonAnimator skeleton_animator_component = Component::Create<SkeletonAnimator>(skeleton_root_entity);
						animation_leaf_node new_animation;
						new_animation.set_animation(resource_manager.FindNamedAnimation(animation_name));
						skeleton_animator_component.SetAnimation(new_animation);
						//TODO: Setting it to be unpaused for the sake of the animation assignment. Otherwise this should be false.
						skeleton_animator_component.SetPaused(false);
					}
				}
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
			(unsigned int)scene_entity_list.size()
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
		if (_id != INVALID_SCENE_ID)
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