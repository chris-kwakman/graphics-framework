#include "Transform.h"

#include <ImGui/imgui.h>

#include <SDL2/SDL_scancode.h>

namespace Component
{

	void TransformManager::impl_clear()
	{
		m_entity_data_index_map.clear();
		m_data_index_entity_map.clear();

		m_local_transforms.clear();
		m_world_matrices.clear();

		m_parent.clear();
		m_first_child.clear();
		m_next_sibling.clear();

		m_root_entities.clear();

		m_dirty_element_count = 0;
	}

	bool TransformManager::impl_create(Entity _e)
	{
		unsigned int const new_index = (unsigned int)m_local_transforms.size();
		// Insert new entity transform data at back index of all data vectors
		m_entity_data_index_map.emplace(_e, new_index);
		m_data_index_entity_map.emplace(new_index, _e);
		
		using math_transform = Engine::Math::transform3D;

		// Initialize default transform data.
		m_local_transforms.push_back(math_transform());
		m_world_matrices.push_back(math_transform().GetMatrix());

		// Set new transform to have no parent, no child and no siblings
		m_parent.push_back(Entity::InvalidEntity);
		m_first_child.push_back(Entity::InvalidEntity);
		m_next_sibling.push_back(Entity::InvalidEntity);

		m_root_entities.insert(_e);

		m_dirty_element_count++;

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
		return m_entity_data_index_map.find(_entity) != m_entity_data_index_map.end();
	}

	void TransformManager::remove_entry(Entity _e)
	{
		uint16_t entity_index = get_entity_index(_e);

		detach_all_entity_children(_e);

		// Swap index data such that entity index data is in back of dirty partition

		// if index is not dirty, swap with back of clean partition.
		if (!check_index_dirty(entity_index) && entity_index != last_clean_index())
		{
			swap_data_indices(entity_index, last_clean_index());
			entity_index = last_clean_index();

			// Swap backmost clean index with backmost dirty index
			swap_data_indices(entity_index, (uint16_t)m_local_transforms.size() - 1);
		}
		// If index IS dirty, swap with back index of dirty partition
		else
		{
			swap_data_indices(entity_index, (uint16_t)m_local_transforms.size() - 1);
			m_dirty_element_count--;
		}

		m_data_index_entity_map.erase(get_entity_index(_e));
		m_entity_data_index_map.erase(_e);

		// Pop back of all data containers
		m_local_transforms.pop_back();
		m_world_matrices.pop_back();
		m_parent.pop_back();
		m_first_child.pop_back();
		m_next_sibling.pop_back();

		m_root_entities.erase(_e);
	}

	uint16_t TransformManager::get_entity_index(Entity _e) const
	{
		return m_entity_data_index_map.at(_e);
	}

	void TransformManager::mark_matrix_dirty(Entity _e)
	{
		mark_index_dirty(m_entity_data_index_map.at(_e));
	}

	void TransformManager::mark_index_dirty(uint16_t _idx)
	{
		// Return early if index is already early
		if (check_index_dirty(_idx))
			return;

		// If not dirty yet, swap with back clean index (unless already in back)
		if (_idx == last_clean_index())
		{
			swap_data_indices(_idx, last_clean_index());
			m_dirty_element_count++;
		}
	}

	bool TransformManager::check_index_dirty(uint16_t _idx) const
	{
		return (m_local_transforms.size() - _idx <= m_dirty_element_count);
	}

	bool TransformManager::attach_entity_to_parent(Entity _entity, Entity _target)
	{
		uint16_t const target_index = get_entity_index(_target);
		uint16_t const entity_index = get_entity_index(_entity);

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
			uint16_t iter_child_index = get_entity_index(iter_child_entity);
			while (m_next_sibling[iter_child_index] != Entity::InvalidEntity)
			{
				iter_child_entity = m_next_sibling[iter_child_index];
				iter_child_index = get_entity_index(iter_child_entity);
			}
			m_next_sibling[iter_child_index] = _entity;
		}

		// Maintain world transform of entity that we are attaching to parent
		auto parent_component = Get(m_parent[entity_index]);
		Engine::Math::transform3D const parent_transform = parent_component.ComputeWorldTransform();
		m_local_transforms[entity_index] = parent_transform.GetInverse() * m_local_transforms[entity_index];

		mark_matrix_dirty(_entity);

		m_root_entities.erase(_entity);

		return true;
	}

	/*
	* Detach input entity from parent (if it's attached to one)
	* @param	Entity		(Child) entity to detach
	*/
	void TransformManager::detach_entity_from_parent(Entity _entity)
	{
		uint16_t const entity_index = get_entity_index(_entity);
		Entity const parent = m_parent[entity_index];

		// Return early if child has no parent to detach from.
		if (parent == Entity::InvalidEntity)
			return;

		uint16_t const parent_index = get_entity_index(parent);

		// Return early if child does not have a parent to detach from
		if (parent == Entity::InvalidEntity)
			return;

		// Remove child from children list

		// If given entity is first child of parent and has a next sibling that is valid,
		// make next sibling new first child of parent
		if (m_first_child[parent_index] == _entity)
		{
			m_first_child[parent_index] = m_next_sibling[entity_index];
			m_next_sibling[entity_index] = Entity::InvalidEntity;
		}

		// If entity is NOT first child, remove from linked list. If entity has next sibling,
		// link next sibling to previous sibling.
		else
		{
			Entity child_iter = m_first_child[parent_index];
			uint16_t child_iter_index = get_entity_index(child_iter);
			// No bounds checking since we know that entity is guaranteed to be a child of parent.
			while (m_next_sibling[child_iter_index] != _entity)
			{
				child_iter = m_next_sibling[child_iter_index];
				child_iter_index = get_entity_index(child_iter);
			}
			m_next_sibling[child_iter_index] = m_next_sibling[entity_index];
		}

		// Maintain world transform of entity that we are detaching from parent
		Engine::Math::transform3D const parent_transform = Get(m_parent[entity_index]).ComputeWorldTransform();
		m_local_transforms[entity_index] = parent_transform * m_local_transforms[entity_index];

		m_parent[entity_index] = Entity::InvalidEntity;
		m_next_sibling[entity_index] = Entity::InvalidEntity;

		m_root_entities.insert(_entity);

		mark_matrix_dirty(_entity);
	}

	void TransformManager::detach_all_entity_children(Entity _entity)
	{
		unsigned int entity_index = get_entity_index(_entity);
		// Detach all children
		Entity child_iter = m_first_child[entity_index];
		while (child_iter != Entity::InvalidEntity)
		{
			detach_entity_from_parent(child_iter);
			child_iter = m_next_sibling[get_entity_index(child_iter)];
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
			entity_iter = m_parent[get_entity_index(entity_iter)];
		}
		return entity_iter == _target;
	}

	/*
	* Swap element data at index 1 and index 2 with each other.
	* This is used for maintaining the correct update order when
	* computing matrices, or when removing entities
	* @param	uint16_t	Index 1
	* @param	uint16_t	Index 2
	*/
	void TransformManager::swap_data_indices(uint16_t _idx1, uint16_t _idx2)
	{
		// Update double-linked entity-index map
		Entity const e1 = m_data_index_entity_map[_idx1];
		Entity const e2 = m_data_index_entity_map[_idx2];
		m_data_index_entity_map[_idx1] = e2;
		m_data_index_entity_map[_idx2] = e1;
		m_entity_data_index_map[e1] = _idx2;
		m_entity_data_index_map[e2] = _idx1;

		// Swap transform data
		std::swap(m_local_transforms[_idx1], m_local_transforms[_idx2]);
		std::swap(m_world_matrices[_idx1], m_world_matrices[_idx2]);
		std::swap(m_parent[_idx1], m_parent[_idx2]);
		std::swap(m_first_child[_idx1], m_first_child[_idx2]);
		std::swap(m_next_sibling[_idx1], m_next_sibling[_idx2]);
	}

	/*
	* Returns last clean index in internal data list.
	* @returns	uint16_t	Back index of clean partition
	*/
	uint16_t TransformManager::last_clean_index() const
	{
		return (uint16_t)m_local_transforms.size() - m_dirty_element_count - 1;
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

		if (ImGui::IsKeyDown(SDL_SCANCODE_DELETE))
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
		if (ImGui::Button("Destroy"))
			_e.DestroyEndOfFrame();
	}

	void TransformManager::display_node_recursively(Entity _e)
	{
		auto & transform_manager = Singleton<TransformManager>();
		unsigned int const entity_index = transform_manager.get_entity_index(_e);
		ImGuiTreeNodeFlags entity_node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick;

		if (!transform_manager.Get(_e).HasChildren())
			entity_node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

		auto& selected_entities = m_editor_scene_graph_data.selected_entities;

		if (selected_entities.find(_e) != selected_entities.end())
			entity_node_flags |= ImGuiTreeNodeFlags_Selected;

		bool const node_open = ImGui::TreeNodeEx((void*)_e.ID(), entity_node_flags, "%d", _e.ID());

		if (ImGui::IsItemClicked())
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
			ImGui::EndDragDropTarget();
		}

		if (node_open)
		{
			// Iterate through all children
			Entity child_iter = transform_manager.m_first_child[entity_index];
			unsigned int child_index = -1;
			while (child_iter != Entity::InvalidEntity)
			{
				child_index = transform_manager.get_entity_index(child_iter);
				display_node_recursively(child_iter);
				child_iter = transform_manager.m_next_sibling[child_index];
			}

			ImGui::TreePop();
		}
	}

	void TransformManager::impl_edit_component(Entity _entity)
	{
		Engine::Math::transform3D& transform = m_local_transforms[get_entity_index(_entity)];
		ImGui::DragFloat3("Position", &transform.position.x, 1.0f, -FLT_MAX / INT_MIN, FLT_MAX / INT_MIN);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.0f, FLT_MAX / INT_MIN);
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
		return GetManager().m_parent[GetManager().get_entity_index(m_owner)];
	}

	void Transform::SetParent(Entity _e)
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
			GetManager().attach_entity_to_parent(m_owner, _e);
		}
	}

	void Transform::DetachFromParent()
	{
		if (GetParent() != Entity::InvalidEntity)
			GetManager().detach_entity_from_parent(m_owner);
	}

	void Transform::AttachChild(Entity _e)
	{
		if(_e != Entity::InvalidEntity && _e != m_owner)
			GetManager().Get(_e).SetParent(m_owner);
	}

	Engine::Math::transform3D Transform::GetLocalTransform() const
	{
		return GetManager().m_local_transforms[GetManager().get_entity_index(m_owner)];
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
		unsigned int entity_iter_idx = GetManager().get_entity_index(m_owner);
		Engine::Math::transform3D model_to_world_transform = GetManager().m_local_transforms[entity_iter_idx];
		Entity parent_entity = GetManager().m_parent[entity_iter_idx];
		while (parent_entity != Entity::InvalidEntity)
		{
			unsigned int const parent_index = GetManager().get_entity_index(parent_entity);;
			model_to_world_transform = GetManager().m_local_transforms[parent_index] * model_to_world_transform;
			parent_entity = GetManager().m_parent[parent_index];
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
		unsigned int const owner_index = GetManager().get_entity_index(m_owner);
		// If owner index is NOT dirty, use pre-computed matrix
		if (!GetManager().check_index_dirty(owner_index))
		{
			return GetManager().m_world_matrices[owner_index];
		}
		// If owner index IS dirty, re-compute model to world matrix.
		else
		{
			glm::mat4x4 transformation_matrix = GetManager().m_local_transforms[owner_index].GetMatrix();
			Entity parent_iter = GetManager().m_parent[owner_index];
			unsigned int parent_index = GetManager().get_entity_index(parent_iter);
			while (parent_iter != Entity::InvalidEntity)
			{
				// If parent is NOT dirty, use precomputed matrix and exit early.
				if (!GetManager().check_index_dirty(parent_index))
				{
					transformation_matrix = GetManager().m_world_matrices[parent_index] * transformation_matrix;
					break;
				}
				// Otherwise, keep going up the hierarchy until we reach the end or reach a valid precomputed matrix
				else
				{
					transformation_matrix = GetManager().m_world_matrices[parent_index] * transformation_matrix;
					parent_iter = GetManager().m_parent[parent_index];
					parent_index = GetManager().get_entity_index(parent_iter);
				}
			}
			return transformation_matrix;
		}
	}

	void Transform::SetLocalTransform(Engine::Math::transform3D _value)
	{
		unsigned int entity_index = GetManager().get_entity_index(m_owner);
		GetManager().m_local_transforms[entity_index] = _value;
		GetManager().mark_index_dirty(entity_index);
	}
	void Transform::SetLocalPosition(glm::vec3 _value)
	{
		unsigned int entity_index = GetManager().get_entity_index(m_owner);
		GetManager().m_local_transforms[entity_index].position = _value;
		GetManager().mark_index_dirty(entity_index);
	}
	void Transform::SetLocalScale(glm::vec3 _value)
	{
		unsigned int entity_index = GetManager().get_entity_index(m_owner);
		GetManager().m_local_transforms[entity_index].scale = _value;
		GetManager().mark_index_dirty(entity_index);
	}
	void Transform::SetLocalRotation(glm::quat _value)
	{
		unsigned int entity_index = GetManager().get_entity_index(m_owner);
		GetManager().m_local_transforms[entity_index].quaternion = _value;
		GetManager().mark_index_dirty(entity_index);
	}
	bool Transform::HasChildren() const
	{
		unsigned int entity_index = GetManager().get_entity_index(m_owner);
		auto manager = GetManager();
		return manager.m_first_child[entity_index] != Entity::InvalidEntity;
	}
}