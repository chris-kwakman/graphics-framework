#ifndef COMPONENT_LIGHT_H
#define COMPONENT_LIGHT_H

#include <Engine/ECS/component_manager.h>
#include <Engine/Graphics/manager.h>

namespace Component
{
	using namespace Engine::ECS;
	using texture_handle = Engine::Graphics::ResourceManager::texture_handle;
	using framebuffer_handle = Engine::Graphics::ResourceManager::framebuffer_handle;

	class PointLightManager;
	class DirectionalLightManager;

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

	struct DirectionalLight : public IComp<DirectionalLightManager>
	{
		DECLARE_COMPONENT(DirectionalLight)
		
		float	GetLinearity() const;
		float	GetOccluderDistance() const;
		void	SetOccluderDistance(float _distance);

		glm::vec3 GetColor() const;
		void SetColor(glm::vec3 _color);

		constexpr uint8_t GetPartitionCount() const;
		float	GetPartitionMinDepth(uint8_t _partition, float _near, float _far) const;
		texture_handle GetPartitionShadowMapTexture(uint8_t _partition) const;
		framebuffer_handle GetPartitionFrameBuffer(uint8_t _partition) const;
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

	/*
	* Assume only one directional light can exist at any time.
	*/
	class DirectionalLightManager : public TCompManager<DirectionalLight>
	{
	public:
		static unsigned int constexpr CSM_PARTITION_COUNT = 3;
	private:

		friend struct DirectionalLight;

		Entity m_directional_light_entity = Entity::InvalidEntity;
		glm::vec3 m_light_color = glm::vec3(1.0f);
		// CSM textures sorted from nearest to furthest.
		// Uses mipmap layers to define textures for different cascades
		texture_handle m_cascade_shadow_map_textures[CSM_PARTITION_COUNT];
		framebuffer_handle m_cascade_shadow_map_framebuffers[CSM_PARTITION_COUNT];
		// Size of first shadow map in cascade shadow map.
		// Each subsequent shadow map will have its size halved.
		uint8_t m_pow2_csm_resolution = 12; 
		float m_partition_linearity = 1.0f; // Mixes linear and logarithmic partitioning approach.
		float m_occluder_distance = 0.0f;

		void setup_csm(uint8_t _pow_two_csm_texture_size, uint8_t _partition_count);

		// Inherited via TCompManager
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

	public:

		virtual const char* GetComponentTypeName() const override { return "DirectionalLight"; }
		DirectionalLight GetDirectionalLight() const { return DirectionalLight(m_directional_light_entity); }
	};
}

#endif // !COMPONENT_LIGHT_H
