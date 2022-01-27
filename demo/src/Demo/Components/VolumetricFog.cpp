#include "VolumetricFog.h"

namespace Component
{
	const char* VolumetricFogManager::GetComponentTypeName() const
	{
		return "VolumetricFog";
	}
	void VolumetricFogManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool VolumetricFogManager::impl_create(Entity _e)
	{
		m_data.m_fog_instances.emplace(_e, fog_instance_data());

		return true;
	}
	void VolumetricFogManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
			m_data.m_fog_instances.erase(_entities[i]);
	}
	bool VolumetricFogManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_data.m_fog_instances.find(_entity) != m_data.m_fog_instances.end();
	}
	void VolumetricFogManager::impl_edit_component(Entity _entity)
	{
		auto& instance = m_data.m_fog_instances.at(_entity);
		ImGui::InputFloat("Base Density", &instance.m_base_density, 0.001f, 0.01f, "%.4f");
		ImGui::DragFloat("Density Height Attenuation", &instance.m_density_height_attenuation, 0.01f, 0.0f, 3.0f);
		if (ImGui::CollapsingHeader("Global Fog Variables"))
		{
			ImGui::SliderFloat("Layer Linearity", &m_data.LayerLinearity, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::ColorEdit3("Fog Color", &m_data.FogAlbedo.x);
			ImGui::SliderFloat("Fog Phase Anisotropy", &m_data.FogPhaseAnisotropy, -1.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Scattering Coefficient", &m_data.ScatteringCoefficient, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat("Absorption Coefficient", &m_data.AbsorptionCoefficient, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat3("Wind Direction", &m_data.WindDirection.x, 0.05f, -FLT_MAX, FLT_MAX, "%.2f");
			ImGui::SliderFloat("Noise Scale", &m_data.NoiseScale, 0.01f, 10.0f, "%.2f", 10.0f);
			ImGui::SliderFloat("Noise Min Value", &m_data.NoiseMinValue, 0.0f, m_data.NoiseMaxValue, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Noise Max Value", &m_data.NoiseMaxValue, m_data.NoiseMinValue, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

			glm::ivec3 new_size = m_data.NewVolFogTextureSize;
			new_size = glm::max(new_size, glm::ivec3(32, 32, 32));
			if (ImGui::InputInt3("Volumetric Fog Texture Size", &new_size.x))
			{
				new_size = glm::max(new_size, glm::ivec3(32, 32, 32));
				m_data.NewVolFogTextureSize = new_size;
				m_data.ResetVolFogTextureSize = true;
			}

			ImGui::TreePop();
		}
	}

	VolumetricFogManager::fog_instance_data& VolumetricFogManager::GetEntityFogInstance(Entity _e)
	{
		return m_data.m_fog_instances.at(_e);
	}

	void VolumetricFogManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];
		}
	}

	void VolumetricFogManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
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