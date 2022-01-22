#include "VolumetricFog.h"

namespace Component
{
	const char* VolumetricFogManager::GetComponentTypeName() const
	{
		return "VolumetricFog";
	}
	void VolumetricFogManager::impl_clear()
	{
		m_fog_instances.clear();
	}
	bool VolumetricFogManager::impl_create(Entity _e)
	{
		m_fog_instances.emplace(_e, fog_instance_data());

		return true;
	}
	void VolumetricFogManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
			m_fog_instances.erase(_entities[i]);
	}
	bool VolumetricFogManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_fog_instances.find(_entity) != m_fog_instances.end();
	}
	void VolumetricFogManager::impl_edit_component(Entity _entity)
	{
		auto& instance = m_fog_instances.at(_entity);
		ImGui::InputFloat("Base Density", &instance.m_base_density, 0.001f, 0.01f, "%.4f");
		ImGui::DragFloat("Density Height Attenuation", &instance.m_density_height_attenuation, 0.01f, 0.0f, 3.0f);
		if (ImGui::CollapsingHeader("Global Fog Variables"))
		{
			ImGui::SliderFloat("Layer Linearity", &LayerLinearity, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::ColorEdit3("Fog Color", &FogAlbedo.x);
			ImGui::SliderFloat("Fog Phase Anisotropy", &FogPhaseAnisotropy, -1.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Scattering Coefficient", &ScatteringCoefficient, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat("Absorption Coefficient", &AbsorptionCoefficient, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat3("Wind Direction", &WindDirection.x, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::SliderFloat("Noise Scale", &NoiseScale, 0.01f, 10.0f, "%.2f", 10.0f);
			ImGui::SliderFloat("Noise Min Value", &NoiseMinValue, 0.0f, NoiseMaxValue, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Noise Max Value", &NoiseMaxValue, NoiseMinValue, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

			glm::ivec3 new_size = NewVolFogTextureSize;
			new_size = glm::max(new_size, glm::ivec3(32, 32, 32));
			if (ImGui::InputInt3("Volumetric Fog Texture Size", &new_size.x))
			{
				new_size = glm::max(new_size, glm::ivec3(32, 32, 32));
				NewVolFogTextureSize = new_size;
				ResetVolFogTextureSize = true;
			}

			ImGui::TreePop();
		}
	}
	void VolumetricFogManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}

	VolumetricFogManager::fog_instance_data& VolumetricFogManager::GetEntityFogInstance(Entity _e)
	{
		return m_fog_instances.at(_e);
	}

	void VolumetricFog::SetBaseDensity(float _value)
	{
		GetManager().GetEntityFogInstance(Owner()).m_base_density = _value;
	}
	void VolumetricFog::SetHeightAttenuation(float _value)
	{
		GetManager().GetEntityFogInstance(Owner()).m_density_height_attenuation = _value;
	}
}