#ifndef COMPONENTS_VOLUMETRIC_FOG
#define COMPONENTS_VOLUMETRIC_FOG

#include <Engine/ECS/component_manager.h>

namespace Component
{
	using namespace Engine::ECS;

	class VolumetricFogManager;
	struct VolumetricFog : public IComp<VolumetricFogManager>
	{
		DECLARE_COMPONENT(VolumetricFog)

		void SetBaseDensity(float _value);
		void SetHeightAttenuation(float _value);
	};
	class VolumetricFogManager : public TCompManager<VolumetricFog>
	{
		struct fog_instance_data
		{
			float		m_base_density = 0.1f;
			float		m_density_height_attenuation = 0.0f;

		};

		std::unordered_map<Entity, fog_instance_data, Entity::hash> m_fog_instances;

		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;
		virtual void impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context) override;

	public:

		float		LayerLinearity = 1.0f;
		glm::vec3	FogAlbedo = glm::vec3(1.0f,1.0f,1.0f);
		float		FogPhaseAnisotropy = 0.0f; // [-1,1]
		float		ScatteringCoefficient = 1.0f;
		float		AbsorptionCoefficient = 1.0f;
		glm::uvec3	NewVolFogTextureSize = { 160,90,128 };
		bool		ResetVolFogTextureSize = false;
		auto const& GetAllFogInstances() const { return m_fog_instances; }

		fog_instance_data& GetEntityFogInstance(Entity _e);
	};
}

#endif // !COMPONENTS_VOLUMETRIC_FOG