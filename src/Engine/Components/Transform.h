#ifndef COMPONENT_TRANSFORM_H
#define COMPONENT_TRANSFORM_H

#include <Engine/ECS/component_manager.h>
#include <unordered_map>
#include <Engine/Math/Transform3D.h>

namespace Component
{
	using namespace Engine::ECS;

	class TransformManager;
	struct Transform : public IComp<TransformManager>
	{
		DECLARE_COMPONENT(Transform);

		Entity						GetParent() const;
		void						SetParent(Entity _e);
		void						DetachFromParent();
		void						AttachChild(Entity _e);

		Engine::Math::transform3D	GetLocalTransform() const;
		glm::vec3					GetLocalPosition() const;
		glm::vec3					GetLocalScale() const;
		glm::quat					GetLocalRotation() const;

		Engine::Math::transform3D   ComputeModelToWorldTransform() const;
		glm::mat4x4					ComputeModelToWorldMatrix() const;

		void SetLocalTransform(Engine::Math::transform3D _value);
		void SetLocalPosition(glm::vec3 _value);
		void SetLocalScale(glm::vec3 _value);
		void SetLocalRotation(glm::quat _value);

		bool HasChildren() const;
	};

	class TransformManager : public TCompManager<Transform>
	{
		// Double-linked maps
		std::unordered_map<Entity, uint16_t, Entity::hash>	m_entity_data_index_map;
		std::unordered_map<uint16_t, Entity>				m_data_index_entity_map;

		// Transform data
		std::vector<Engine::Math::transform3D>	m_local_transforms;
		std::vector<glm::mat4x4>				m_world_matrices; // Element not up-to-date if index is dirty
		// Relationship data
		std::vector<Entity>						m_parent;
		std::vector<Entity>						m_first_child;
		std::vector<Entity>						m_next_sibling;

		std::unordered_set<Entity, Entity::hash> m_root_entities;

		friend struct Transform;

		// How many world matrices need to be recomputed from back to front.
		unsigned int m_dirty_element_count = 0;

		void impl_clear() final;
		bool impl_create(Entity _e) final;
		void impl_destroy(Entity const* _entities, unsigned int _count) final;
		bool impl_component_owned_by_entity(Entity _entity) const final;

		void remove_entry(Entity _e);
		uint16_t get_entity_index(Entity _e) const;

		void mark_entity_dirty(Entity _e);
		void mark_index_dirty(uint16_t _idx);
		bool check_index_dirty(uint16_t _idx) const;

		bool attach_entity_to_parent(Entity _entity, Entity _target);
		void detach_entity_from_parent(Entity _child);
		void detach_all_entity_children(Entity _entity);
		bool is_entity_attached_to_entity(Entity _entity, Entity _target) const;

		void swap_data_indices(uint16_t _idx1, uint16_t _idx2);
		uint16_t last_clean_index() const;

	public:

		struct EditorSceneGraphData
		{
			std::unordered_set<Entity, Entity::hash> selected_entities;
		};

		// Inherited via TCompManager
		const char* GetComponentTypeName() const final { return "Transform"; }
		std::vector<Entity> GetRootEntities() const;

		void DisplaySceneGraph();

		EditorSceneGraphData GetEditorSceneGraphData() const { return m_editor_scene_graph_data; }

	private:

		EditorSceneGraphData m_editor_scene_graph_data;

		void display_node_recursively(Entity _e);

		// Inherited via TCompManager
		virtual void impl_edit_component(Entity _entity) override;
	};
}

#endif