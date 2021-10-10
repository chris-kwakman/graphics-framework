#ifndef COMPONENT_LIGHT_H
#define COMPONENT_LIGHT_H

#include <Engine/ECS/component_manager.h>

namespace Component
{

	using namespace Engine::ECS;

	class PointLightManager;
	struct PointLight : public IComp<PointLightManager>
	{
		DECLARE_COMPONENT(PointLight);

		float GetRadius() const;
		void SetRadius(float _radius);

		glm::vec3 GetColor() const;
		void SetColor(glm::vec3 _color);

	private:

		size_t get_light_index() const;
	};

	class PointLightManager : public TCompManager<PointLight>
	{
		friend struct PointLight;

		std::unordered_map<Entity, unsigned int, Entity::hash> m_entity_map;
		// Store light data in SOA format so we can pass many lights at once if we desire to do so.
		std::vector<Entity> m_index_entities;
		std::vector<glm::vec3> m_light_color_arr;
		std::vector<float> m_light_radius_arr;

		// Inherited via TCompManager
		bool impl_create(Entity _e) final;
		void impl_destroy(Entity const* _entities, unsigned int _count) final;
		bool impl_component_owned_by_entity(Entity _entity) const final;
		void impl_edit_component(Entity _entity) final;
		void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) final;

	public:

		struct Collection
		{
			size_t m_light_count;
			glm::vec3 const* m_light_color_arr;
			float const* m_light_radius_arr;
			std::vector<glm::vec3> m_light_pos_arr;
		};

		const char* GetComponentTypeName() const final { return "PointLight"; }
		Collection GetPointLightCollection() const;


		// Inherited via TCompManager
		virtual void impl_clear() override;

	};
}

#endif // !COMPONENT_LIGHT_H
