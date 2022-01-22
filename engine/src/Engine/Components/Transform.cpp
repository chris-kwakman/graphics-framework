#include "Transform.h"


#include <Engine/Editor/editor.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Nameable.h>


#include <ImGui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <SDL2/SDL_scancode.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

//TODO: Make transform.cpp not depend on Sandbox content.
//#include <Sandbox/LoadScene.h>

namespace Component
{

	void TransformManager::impl_clear()
	{
		m_entity_indexer_map.clear();

		m_local_transforms.clear();
		m_parent.clear();
		m_first_child.clear();
		m_next_sibling.clear();

		m_world_matrix_data.clear();
		m_world_matrix_owners.clear();

		m_root_entities.clear();

		m_dirty_matrix_count = 0;
	}

	bool TransformManager::impl_create(Entity _e)
	{
		// Insert new entity transform data at back index of all data vectors
		indexer_data new_indexer;
		new_indexer.transform = (unsigned int)m_local_transforms.size();
		new_indexer.matrix = (unsigned int)m_world_matrix_data.size();

		m_entity_indexer_map.emplace(_e, new_indexer);
		
		using math_transform = Engine::Math::transform3D;

		// Initialize default transform data.
		// Set new transform to have no parent, no child and no siblings
		m_transform_owners.push_back(_e);
		m_local_transforms.push_back(math_transform());
		m_parent.push_back(Entity::InvalidEntity);
		m_first_child.push_back(Entity::InvalidEntity);
		m_next_sibling.push_back(Entity::InvalidEntity);


		m_world_matrix_owners.push_back(_e);
		m_world_matrix_data.push_back(math_transform().GetMatrix());

		m_root_entities.insert(_e);

		m_dirty_matrix_count++;

		return true;
	}

	void TransformManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			if(ComponentOwnedByEntity(_entities[i]))
				remove_entry(_entities[i]);
		}
	}

	bool TransformManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_entity_indexer_map.find(_entity) != m_entity_indexer_map.end();
	}

	void TransformManager::remove_entry(Entity _e)
	{
		indexer_data entity_indexer = get_entity_indexer_data(_e);

		detach_all_entity_children(_e);
		detach_entity_from_parent(_e);

		// Swap matrix indices such that entity matrix data is in back of dirty partition

		// If matrix is not dirty, swap with back of clean partition.
		if (!check_matrix_dirty(entity_indexer.matrix) && entity_indexer.matrix != last_clean_matrix_index())
		{
			swap_matrix_indices(entity_indexer.matrix, last_clean_matrix_index());
			entity_indexer.matrix = last_clean_matrix_index();

			// Swap backmost clean index with backmost dirty index
			swap_matrix_indices(entity_indexer.matrix, (uint16_t)m_local_transforms.size() - 1);
		}
		// If index IS dirty, swap with back index of dirty partition
		else
		{
			swap_matrix_indices(entity_indexer.matrix, (uint16_t)m_local_transforms.size() - 1);
			m_dirty_matrix_count--;
		}

		swap_transform_index_to_back(entity_indexer.transform);

		m_entity_indexer_map.erase(_e);

		// Remove from root entity list
		m_root_entities.erase(_e);

		// Pop back of all data containers
		m_transform_owners.pop_back();
		m_local_transforms.pop_back();
		m_parent.pop_back();
		m_first_child.pop_back();
		m_next_sibling.pop_back();
		// Pop back all matrix data contianers
		m_world_matrix_owners.pop_back();
		m_world_matrix_data.pop_back();

		m_root_entities.erase(_e);
	}

	TransformManager::indexer_data TransformManager::get_entity_indexer_data(Entity _e) const
	{
		return m_entity_indexer_map.at(_e);
	}

	/*
	* Swap transform data at given index with that of back index.
	* @param	uint16_t		Index to swap with back
	*/
	void TransformManager::swap_transform_index_to_back(uint16_t _idx)
	{
		if (m_local_transforms.empty() || _idx == m_local_transforms.size() - 1)
			return;

		unsigned int const back_entity_idx = (unsigned int)m_transform_owners.size() - 1u;
		Entity const index_entity = m_transform_owners[_idx];
		Entity const back_entity = m_transform_owners[back_entity_idx];

		// Update indexers in entity to indexer map
		std::swap(m_entity_indexer_map.at(index_entity).transform, m_entity_indexer_map.at(back_entity).transform);
		// Swap data at indices in actual transform data vectors.
		std::swap(m_local_transforms[_idx], m_local_transforms[back_entity_idx]);
		std::swap(m_transform_owners[_idx], m_transform_owners[back_entity_idx]);
		std::swap(m_parent[_idx], m_parent[back_entity_idx]);
		std::swap(m_first_child[_idx], m_first_child[back_entity_idx]);
		std::swap(m_next_sibling[_idx], m_next_sibling[back_entity_idx]);
	}

	void TransformManager::mark_matrix_dirty(Entity _e)
	{
		mark_matrix_dirty(get_entity_indexer_data(_e).matrix);
	}

	void TransformManager::mark_matrix_dirty(uint16_t _idx)
	{
		// Return early if index is already early
		if (check_matrix_dirty(_idx))
			return;

		// If not dirty yet, swap with back clean index (unless already in back)
		if (_idx == last_clean_matrix_index())
		{
			swap_matrix_indices(_idx, last_clean_matrix_index());
			m_dirty_matrix_count++;
		}
	}

	bool TransformManager::check_matrix_dirty(uint16_t _idx) const
	{
		return (m_local_transforms.size() - _idx <= m_dirty_matrix_count);
	}

	bool TransformManager::attach_entity_to_parent(Entity _entity, Entity _target, bool _maintainWorldTransform)
	{
		uint16_t const target_index = get_entity_indexer_data(_target).transform;
		uint16_t const entity_index = get_entity_indexer_data(_entity).transform;

		// Return early if given child entity is already attached to parent.
		if (m_parent[entity_index] == _target)
			return false;

		m_parent[entity_index] = _target;

		// Set given child as first child if parent does not have a child yet.
		if (m_first_child[target_index] == Entity::InvalidEntity)
			m_first_child[target_index] = _entity;
		else
		{
			// Iterate through linked sibling list
			Entity iter_child_entity = m_first_child[target_index];
			uint16_t iter_child_index = get_entity_indexer_data(iter_child_entity).transform;
			while (m_next_sibling[iter_child_index] != Entity::InvalidEntity)
			{
				iter_child_entity = m_next_sibling[iter_child_index];
				iter_child_index = get_entity_indexer_data(iter_child_entity).transform;
			}
			m_next_sibling[iter_child_index] = _entity;
		}

		// Maintain world transform of entity that we are attaching to parent
		if (_maintainWorldTransform)
		{
			auto parent_component = Get(m_parent[entity_index]);
			Engine::Math::transform3D const parent_transform = parent_component.ComputeWorldTransform();
			m_local_transforms[entity_index] = parent_transform.GetInverse() * m_local_transforms[entity_index];
		}

		mark_matrix_dirty(_entity);

		m_root_entities.erase(_entity);

		return true;
	}

	/*
	* Detach input entity from parent (if it's attached to one)
	* @param	Entity		(Child) entity to detach
	*/
	void TransformManager::detach_entity_from_parent(Entity _entity, bool _maintainWorldTransform)
	{
		uint16_t const entity_transform_index = get_entity_indexer_data(_entity).transform;
		Entity const parent = m_parent[entity_transform_index];

		// Return early if child has no parent to detach from.
		if (parent == Entity::InvalidEntity)
			return;

		uint16_t const parent_index = get_entity_indexer_data(parent).transform;

		// Remove child from children list

		// If given entity is first child of parent and has a next sibling that is valid,
		// make next sibling new first child of parent
		if (m_first_child[parent_index] == _entity)
		{
			m_first_child[parent_index] = m_next_sibling[entity_transform_index];
			m_next_sibling[entity_transform_index] = Entity::InvalidEntity;
		}

		// If entity is NOT first child, remove from linked list. If entity has next sibling,
		// link next sibling to previous sibling.
		else
		{
			Entity child_iter = m_first_child[parent_index];
			uint16_t child_iter_index = get_entity_indexer_data(child_iter).transform;
			// No bounds checking since we know that entity is guaranteed to be a child of parent.
			while (m_next_sibling[child_iter_index] != _entity)
			{
				child_iter = m_next_sibling[child_iter_index];
				child_iter_index = get_entity_indexer_data(child_iter).transform;
			}
			m_next_sibling[child_iter_index] = m_next_sibling[entity_transform_index];
		}

		// Maintain world transform of entity that we are detaching from parent
		if (_maintainWorldTransform)
		{
			Engine::Math::transform3D const parent_transform = Get(m_parent[entity_transform_index]).ComputeWorldTransform();
			m_local_transforms[entity_transform_index] = parent_transform * m_local_transforms[entity_transform_index];
		}

		m_parent[entity_transform_index] = Entity::InvalidEntity;
		m_next_sibling[entity_transform_index] = Entity::InvalidEntity;

		m_root_entities.insert(_entity);

		mark_matrix_dirty(_entity);
	}

	void TransformManager::detach_all_entity_children(Entity _entity, bool _maintainWorldTransform)
	{
		unsigned int entity_index = get_entity_indexer_data(_entity).transform;
		// Detach all children
		Entity child_iter = m_first_child[entity_index];
		while (child_iter != Entity::InvalidEntity)
		{
			Entity next_child_iter = m_next_sibling[get_entity_indexer_data(child_iter).transform];
			detach_entity_from_parent(child_iter, _maintainWorldTransform);
			child_iter = next_child_iter;
		}
		m_first_child[entity_index] = Entity::InvalidEntity;
	}

	/*
	* Performs upwards check to see if entity is a (grand-)child of target.
	* @param	Entity		Entity performing check
	* @param	Entity		Target that we're looking for
	*/
	bool TransformManager::is_entity_attached_to_entity(Entity _entity, Entity _target) const
	{
		Entity entity_iter = _entity;
		while (!(entity_iter == Entity::InvalidEntity || entity_iter == _target))
		{
			entity_iter = m_parent[get_entity_indexer_data(entity_iter).transform];
		}
		return entity_iter == _target;
	}

	/*
	* Swap matrices at input indices with each other
	* This is used for maintaining the correct update order when
	* computing matrices, or when removing entities
	* @param	uint16_t	Index 1
	* @param	uint16_t	Index 2
	*/
	void TransformManager::swap_matrix_indices(uint16_t _idx1, uint16_t _idx2)
	{
		// Update bi-directional entity-matrix-index map
		Entity const e1 = m_world_matrix_owners[_idx1];
		Entity const e2 = m_world_matrix_owners[_idx2];
		m_world_matrix_owners[_idx1] = e2;
		m_world_matrix_owners[_idx2] = e1;
		m_entity_indexer_map[e1].matrix = _idx2;
		m_entity_indexer_map[e2].matrix = _idx1;

		// Swap matrix data
		std::swap(m_world_matrix_data[_idx1], m_world_matrix_data[_idx2]);
	}

	/*
	* Returns last clean index in internal data list.
	* @returns	uint16_t	Back index of clean partition
	*/
	uint16_t TransformManager::last_clean_matrix_index() const
	{
		return (uint16_t)m_local_transforms.size() - m_dirty_matrix_count - 1;
	}


	//////////////////////////////////////////////////////////////////////////
	//					Manager Public Methods
	//////////////////////////////////////////////////////////////////////////

	std::vector<Entity> TransformManager::GetRootEntities() const
	{
		std::vector<Entity> entity_list;
		entity_list.reserve(m_root_entities.size());
		for (Entity e : m_root_entities)
			entity_list.push_back(e);
		return entity_list;
	}

	void TransformManager::DisplaySceneGraph()
	{
		auto root_nodes = Singleton<TransformManager>().GetRootEntities();

		if (ImGui::BeginChild("GraphDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (auto root_entity : root_nodes)
				display_node_recursively(root_entity);

			if (ImGui::BeginPopupContextWindow("EntityHierarchyContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
			{
				if (ImGui::Button("Create Entity"))
				{
					Entity new_entity;
					if (Singleton<EntityManager>().EntityCreationRequest(&new_entity, 1))
					{
						Create(new_entity);
					}
					else
					{
						ImGui::BeginPopup("EntityCreationError");
						ImGui::Text("ERROR: Failed to create entity.");
						ImGui::EndPopup();
					}
				}

				ImGui::EndPopup();
			}
		}
		ImGui::EndChild();

		if (ImGui::IsWindowFocused() && ImGui::IsKeyDown(SDL_SCANCODE_DELETE))
		{
			for (auto entity : m_editor_scene_graph_data.selected_entities)
				entity.DestroyEndOfFrame();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("ENTITY"))
			{
				Singleton<TransformManager>().Get(*reinterpret_cast<Entity*>(payload->Data)).DetachFromParent();
			}
			ImGui::EndDragDropTarget();
		}
	}

	static void entity_node_context(Entity _e)
	{
		uint8_t const MAX_STRING_SIZE = Component::Nameable::comp_manager::MAX_STRING_SIZE;
		static char name_buffer[MAX_STRING_SIZE];

		if (ImGui::IsWindowAppearing()) {
			strncpy(name_buffer, _e.GetName(), MAX_STRING_SIZE);
		}

		if (ImGui::InputText("Edit Name", name_buffer, MAX_STRING_SIZE))
			_e.SetName(name_buffer);

		if (ImGui::Button("Destroy"))
		{
			std::vector<Entity> entities_to_delete, children_to_delete;
			children_to_delete = _e.GetComponent<Transform>().GetChildren();
			entities_to_delete.push_back(_e);
			entities_to_delete.insert(entities_to_delete.end(), children_to_delete.begin(), children_to_delete.end());
			_e.DestroyEndOfFrame();
			for (unsigned int i = 0; i < entities_to_delete.size(); ++i)
			{
				auto more_children_to_delete = entities_to_delete[i].GetComponent<Transform>().GetChildren();
				entities_to_delete.insert(
					entities_to_delete.end(), 
					more_children_to_delete.begin(), more_children_to_delete.end()
				);
			}
			Singleton<EntityManager>().EntityDelayedDeletion(&entities_to_delete.front(), (unsigned int)entities_to_delete.size());
		}

		ImGui::Text("Entity ID: %d", _e.ID());
	}

	void TransformManager::display_node_recursively(Entity _e)
	{
		auto & transform_manager = Singleton<TransformManager>();
		unsigned int const entity_transform_index = transform_manager.get_entity_indexer_data(_e).transform;
		ImGuiTreeNodeFlags entity_node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick;

		if (!transform_manager.Get(_e).HasChildren())
			entity_node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

		auto& selected_entities = m_editor_scene_graph_data.selected_entities;

		if (selected_entities.find(_e) != selected_entities.end())
			entity_node_flags |= ImGuiTreeNodeFlags_Selected;

		bool const node_open = ImGui::TreeNodeEx((void*)_e.ID(), entity_node_flags, "%s", _e.GetName());

		if (ImGui::IsItemFocused() && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (ImGui::IsKeyDown(SDL_SCANCODE_LCTRL))
			{
				if (selected_entities.find(_e) == selected_entities.end())
					selected_entities.insert(_e);
				else
					selected_entities.erase(_e);
			}
			else
			{
				selected_entities.clear();
				selected_entities.insert(_e);
			}
		}

		if (ImGui::BeginPopupContextItem())
		{
			entity_node_context(_e);
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource())
		{
			ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceAutoExpirePayload;
			ImGui::SetDragDropPayload("ENTITY", &_e, sizeof(_e));
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			ImGuiDragDropFlags target_flags = ImGuiDragDropFlags_None;
			if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("ENTITY", target_flags))
			{
				Entity source_entity = *reinterpret_cast<Entity*>(payload->Data);
				transform_manager.Get(_e).AttachChild(source_entity);
			}
			// TODO: Perform this via some callback maybe?
			else if (ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("RESOURCE_MODEL", target_flags))
			{
				assert(false && "This needs to be re-worked!");
				//const char* model_filepath = *reinterpret_cast<const char**>(payload->Data);
				//nlohmann::json const scene = Sandbox::LoadJSON(model_filepath);
				//auto model_scene_nodes = Sandbox::LoadGLTFScene(scene, model_filepath, nullptr);
				//Transform e_transform = _e.GetComponent<Transform>();
				//for (Entity model_scene_node : model_scene_nodes)
				//	e_transform.AttachChild(model_scene_node, false);
			}
			ImGui::EndDragDropTarget();
		}

		if (node_open)
		{
			// Iterate through all children
			Entity child_iter = transform_manager.m_first_child[entity_transform_index];
			unsigned int child_transform_index = -1;
			while (child_iter != Entity::InvalidEntity)
			{
				child_transform_index = transform_manager.get_entity_indexer_data(child_iter).transform;
				display_node_recursively(child_iter);
				child_iter = transform_manager.m_next_sibling[child_transform_index];
			}

			ImGui::TreePop();
		}
	}

	void TransformManager::impl_edit_component(Entity _entity)
	{
		static ImGuizmo::OPERATION s_imguizmo_current_operation = ImGuizmo::TRANSLATE;
		static ImGuizmo::MODE s_imguizmo_current_mode = ImGuizmo::LOCAL;

		//if (ImGui::IsItemFocused())
			Singleton<Engine::Editor::Editor>().ComponentUsingImguizmoWidget = GetComponentTypeName();

		Transform transform_component = Get(_entity);

		auto editor = Singleton<Engine::Editor::Editor>();
		Transform const editor_cam_transform = editor.EditorCameraEntity.GetComponent<Transform>();
		Camera const editor_cam_camera = editor.EditorCameraEntity.GetComponent<Camera>();

		glm::mat4x4 const matrix_view = editor_cam_transform.ComputeWorldTransform().GetInvMatrix();
		glm::mat4x4 const matrix_perspective = editor_cam_camera.GetCameraData().get_perspective_matrix();

		glm::mat4x4 transform_matrix;
		if (s_imguizmo_current_mode == ImGuizmo::WORLD)
			transform_matrix = transform_component.ComputeWorldTransform().GetMatrix();
		else
			transform_matrix = transform_component.GetLocalTransform().GetMatrix();

		bool is_manipulated = false;
		is_manipulated = ImGuizmo::Manipulate(
			&matrix_view[0][0],
			&matrix_perspective[0][0],
			s_imguizmo_current_operation,
			s_imguizmo_current_mode,
			&transform_matrix[0][0]
		);

		if (is_manipulated)
		{
			Engine::Math::transform3D transform;
			glm::vec3 skew;
			glm::vec4 perspective;

			glm::decompose(transform_matrix, transform.scale, transform.quaternion, transform.position, skew, perspective);

			if (s_imguizmo_current_mode == ImGuizmo::LOCAL)
				transform_component.SetLocalTransform(transform);
			else
			{
				transform_component.SetLocalTransform(
					(transform_component.ComputeWorldTransform() * transform_component.GetLocalTransform().GetInverse()).GetInverse()
					* transform
				);
			}
		}

		const char* name_gizmo_operation = s_imguizmo_current_operation == ImGuizmo::TRANSLATE
			? "Translate"
			: s_imguizmo_current_operation == ImGuizmo::SCALE
			? "Scale"
			: "Rotate";
		if(ImGui::BeginCombo("Gizmo Operation", name_gizmo_operation))
		{
			if (ImGui::Selectable("Translate")) s_imguizmo_current_operation = ImGuizmo::TRANSLATE;
			if (ImGui::Selectable("Scale")) s_imguizmo_current_operation = ImGuizmo::SCALE;
			if (ImGui::Selectable("Rotate")) s_imguizmo_current_operation = ImGuizmo::ROTATE;
			ImGui::EndCombo();
		}
		const char* name_gizmo_mode = s_imguizmo_current_mode == ImGuizmo::WORLD
			? "World" : "Local";
		if (ImGui::BeginCombo("Gizmo Mode", name_gizmo_mode))
		{
			if (ImGui::Selectable("World")) s_imguizmo_current_mode = ImGuizmo::WORLD;
			if (ImGui::Selectable("Local")) s_imguizmo_current_mode = ImGuizmo::LOCAL;
			ImGui::EndCombo();
		}

		Engine::Math::transform3D transform;
		if (s_imguizmo_current_mode == ImGuizmo::WORLD)
			transform = transform_component.ComputeWorldTransform();
		else
			transform = m_local_transforms[get_entity_indexer_data(_entity).transform];

		ImGui::DragFloat3("Position", &transform.position.x, 1.0f, -FLT_MAX / INT_MIN, FLT_MAX / INT_MIN);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.0f, FLT_MAX / INT_MIN);
		ImGui::DragFloat4("Orientation", &transform.quaternion.x, 0.025f);
		transform.quaternion = glm::normalize(transform.quaternion);

		if (!is_manipulated)
		{
			if(s_imguizmo_current_mode == ImGuizmo::LOCAL)
				transform_component.SetLocalTransform(transform);
			else
			{
				transform_component.SetLocalTransform(
					(transform_component.ComputeWorldTransform() * transform_component.GetLocalTransform().GetInverse()).GetInverse()
					* transform
				);
			}
		}
	}

	/*
	* Deserialize json into this entity's component
	* @param	Entity
	* @param	json		Json component to deserialize
	* @param	SceneContext		Provided when deserialising scene
	* @details	Assumes entity corresponding to this component has already been created.
	*/
	void TransformManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
		Transform component = Get(_e);
		component.SetLocalTransform(_json_comp["local_transform"]);
		if (_context)
		{
			auto parent_iter = _json_comp.find("parent_index");
			if (parent_iter != _json_comp.end())
				component.SetParent(_context->m_index_entities[parent_iter->get<unsigned int>()]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//			Component Getters / Setters
	//////////////////////////////////////////////////////////////////////////


	Entity Transform::GetParent() const
	{
		return GetManager().m_parent[GetManager().get_entity_indexer_data(m_owner).transform];
	}

	void Transform::SetParent(Entity _e, bool _maintainWorldTransform)
	{
		// Ignore if:
		// - Given target is self
		// - Attempting to attach self to target and self is "above" target
		if (
			GetParent() != _e &&
			m_owner != _e && 
			!GetManager().is_entity_attached_to_entity(_e, m_owner))
		{
			DetachFromParent();
			GetManager().attach_entity_to_parent(m_owner, _e, _maintainWorldTransform);
		}
	}

	void Transform::DetachFromParent(bool _maintainWorldTransform)
	{
		if (GetParent() != Entity::InvalidEntity)
			GetManager().detach_entity_from_parent(m_owner, _maintainWorldTransform);
	}

	void Transform::AttachChild(Entity _e, bool _maintainWorldTransform)
	{
		if(_e != Entity::InvalidEntity && _e != m_owner)
			GetManager().Get(_e).SetParent(m_owner, _maintainWorldTransform);
	}

	Engine::Math::transform3D Transform::GetLocalTransform() const
	{
		return GetManager().m_local_transforms[GetManager().get_entity_indexer_data(m_owner).transform];
	}
	glm::vec3 Transform::GetLocalPosition() const
	{
		return GetLocalTransform().position;
	}
	glm::vec3 Transform::GetLocalScale() const
	{
		return GetLocalTransform().scale;
	}
	glm::quat Transform::GetLocalRotation() const
	{
		return GetLocalTransform().quaternion;
	}

	/*
	* Computes model to world transform using entity hierarchy.
	* @returns	Transform 3D
	*/
	Engine::Math::transform3D Transform::ComputeWorldTransform() const
	{
		unsigned int entity_iter_transform_idx = GetManager().get_entity_indexer_data(m_owner).transform;
		Engine::Math::transform3D model_to_world_transform = GetManager().m_local_transforms[entity_iter_transform_idx];
		Entity parent_entity = GetManager().m_parent[entity_iter_transform_idx];
		while (parent_entity != Entity::InvalidEntity)
		{
			unsigned int const parent_transform_index = GetManager().get_entity_indexer_data(parent_entity).transform;
			model_to_world_transform = GetManager().m_local_transforms[parent_transform_index] * model_to_world_transform;
			parent_entity = GetManager().m_parent[parent_transform_index];
		}
		return model_to_world_transform;
	}

	/*
	* Gets model to world matrix using precomputed matrix.
	* If precomputed matrix is outdated (dirty), recompute from scratch using model to world transform.
	* @returns	Model to World matrix
	*/
	glm::mat4x4 Transform::ComputeWorldMatrix() const
	{
		using indexer_data = TransformManager::indexer_data;
		indexer_data owner_indexer = GetManager().get_entity_indexer_data(m_owner);
		// If owner index is NOT dirty, use pre-computed matrix
		if (!GetManager().check_matrix_dirty(owner_indexer.matrix))
		{
			return GetManager().m_world_matrix_data[owner_indexer.matrix];
		}
		// If owner index IS dirty, re-compute model to world matrix.
		else
		{
			glm::mat4x4 transformation_matrix = GetManager().m_local_transforms[owner_indexer.transform].GetMatrix();
			Entity parent_iter = GetManager().m_parent[owner_indexer.transform];
			if (parent_iter != Entity::InvalidEntity)
			{
				while (parent_iter != Entity::InvalidEntity)
				{
					indexer_data parent_indexer = GetManager().get_entity_indexer_data(parent_iter);
					// If parent is NOT dirty, use precomputed matrix and exit early.
					if (!GetManager().check_matrix_dirty(parent_indexer.matrix))
					{
						transformation_matrix = GetManager().m_world_matrix_data[parent_indexer.matrix] * transformation_matrix;
						break;
					}
					// Otherwise, keep going up the hierarchy until we reach the end or reach a valid precomputed matrix
					else
					{
						transformation_matrix = GetManager().m_world_matrix_data[parent_indexer.matrix] * transformation_matrix;
						parent_iter = GetManager().m_parent[parent_indexer.transform];
					}
				}
			}
			return transformation_matrix;
		}
	}

	void Transform::SetLocalTransform(Engine::Math::transform3D _value)
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		GetManager().m_local_transforms[entity_transform_index] = _value;
		GetManager().mark_matrix_dirty(entity_transform_index);
	}
	void Transform::SetLocalPosition(glm::vec3 _value)
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		GetManager().m_local_transforms[entity_transform_index].position = _value;
		GetManager().mark_matrix_dirty(entity_transform_index);
	}
	void Transform::SetLocalScale(glm::vec3 _value)
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		GetManager().m_local_transforms[entity_transform_index].scale = _value;
		GetManager().mark_matrix_dirty(entity_transform_index);
	}
	void Transform::SetLocalRotation(glm::quat _value)
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		GetManager().m_local_transforms[entity_transform_index].quaternion = _value;
		GetManager().mark_matrix_dirty(entity_transform_index);
	}
	bool Transform::HasChildren() const
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		auto manager = GetManager();
		return manager.m_first_child[entity_transform_index] != Entity::InvalidEntity;
	}
	std::vector<Entity> Transform::GetChildren() const
	{
		unsigned int const entity_transform_index = GetManager().get_entity_indexer_data(m_owner).transform;
		auto manager = GetManager();
		std::vector<Entity> children;
		Entity child_iter = manager.m_first_child[entity_transform_index];
		while (child_iter != Entity::InvalidEntity)
		{
			children.push_back(child_iter);
			child_iter = manager.m_next_sibling[manager.get_entity_indexer_data(child_iter).transform];
		}
		return children;
	}
}