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

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(fog_instance_data, m_base_density, m_density_height_attenuation)
		};

		struct manager_data
		{
			std::unordered_map<Entity, fog_instance_data, Entity::hash> m_fog_instances;

			float		LayerLinearity = 1.0f;
			glm::vec3	FogAlbedo = glm::vec3(1.0f,1.0f,1.0f);
			float		FogPhaseAnisotropy = 0.0f; // [-1,1]
			float		ScatteringCoefficient = 1.0f;
			float		AbsorptionCoefficient = 1.0f;
			glm::uvec3	NewVolFogTextureSize = { 160,90,128 };
			bool		ResetVolFogTextureSize = false;
			glm::vec3	WindDirection = glm::vec3(0.0f);
			float		NoiseScale = 0.1f;
			float		NoiseMinValue = 0.5f;
			float		NoiseMaxValue = 1.0f;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(
				manager_data,
				m_fog_instances,
				LayerLinearity,
				FogAlbedo,
				FogPhaseAnisotropy,
				ScatteringCoefficient,
				AbsorptionCoefficient,
				NewVolFogTextureSize,
				ResetVolFogTextureSize,
				WindDirection,
				NoiseScale,
				NoiseMinValue,
				NoiseMaxValue
			)
		};


		// Inherited via TCompManager
		virtual const char* GetComponentTypeName() const override;
		virtual void impl_clear() override;
		virtual bool impl_create(Entity _e) override;
		virtual void impl_destroy(Entity const* _entities, unsigned int _count) override;
		virtual bool impl_component_owned_by_entity(Entity _entity) const override;
		virtual void impl_edit_component(Entity _entity) override;

	public:

		manager_data m_data;

		auto const& GetAllFogInstances() const { return m_data.m_fog_instances; }

		fog_instance_data& GetEntityFogInstance(Entity _e);

		// Inherited via TCompManager
		virtual void impl_deserialize_data(nlohmann::json const& _j) override;
		virtual void impl_serialize_data(nlohmann::json& _j) const override;
	};
}

#endif // !COMPONENTS_VOLUMETRIC_FOG