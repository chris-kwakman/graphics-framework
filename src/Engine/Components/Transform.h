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

		Engine::Math::transform3D   ComputeWorldTransform() const;
		glm::mat4x4					ComputeWorldMatrix() const;

		void SetLocalTransform(Engine::Math::transform3D _value);
		void SetLocalPosition(glm::vec3 _value);
		void SetLocalScale(glm::vec3 _value);
		void SetLocalRotation(glm::quat _value);

		bool HasChildren() const;
	};

	class TransformManager : public TCompManager<Transform>
	{
		struct indexer_data
		{
			unsigned int transform;
			unsigned int matrix;
		};

		// Double-linked maps
		std::unordered_map<Entity, indexer_data, Entity::hash>	m_entity_indexer_map;

		// Transform and relationship data
		// Referred to by indexer_data/transform variable
		std::vector<Entity>						m_transform_owners;
		std::vector<Engine::Math::transform3D>	m_local_transforms;
		std::vector<Entity>						m_parent;
		std::vector<Entity>						m_first_child;
		std::vector<Entity>						m_next_sibling;

		// Referred to by indexer_data/matrix variable
		std::vector<Entity>						m_world_matrix_owners;
		std::vector<glm::mat4x4>				m_world_matrix_data;
		// How many world matrices need to be recomputed from back to front.
		unsigned int m_dirty_matrix_count = 0;

		std::unordered_set<Entity, Entity::hash> m_root_entities;

		friend struct Transform;


		void impl_clear() final;
		bool impl_create(Entity _e) final;
		void impl_destroy(Entity const* _entities, unsigned int _count) final;
		bool impl_component_owned_by_entity(Entity _entity) const final;

		void remove_entry(Entity _e);
		indexer_data get_entity_indexer_data(Entity _e) const;

		void swap_transform_index_to_back(uint16_t _idx);

		void mark_matrix_dirty(Entity _e);
		void mark_matrix_dirty(uint16_t _idx);
		bool check_matrix_dirty(uint16_t _idx) const;
		void swap_matrix_indices(uint16_t _idx1, uint16_t _idx2);
		uint16_t last_clean_matrix_index() const;

		bool attach_entity_to_parent(Entity _entity, Entity _target);
		void detach_entity_from_parent(Entity _child);
		void detach_all_entity_children(Entity _entity);
		bool is_entity_attached_to_entity(Entity _entity, Entity _target) const;


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
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const * _context) override;
	};
}

#endif