#include "Light.h"
#include <Engine/Components/Transform.h>

namespace Component
{

	//////////////////////////////////////////////////////////////////
	//					Point Light Manager
	//////////////////////////////////////////////////////////////////

	bool PointLightManager::impl_create(Entity _e)
	{
		unsigned int const new_light_index = (unsigned int)m_light_color_arr.size();
		m_entity_map.emplace(_e, new_light_index);
		m_light_color_arr.emplace_back(1.0f, 1.0f, 1.0f);
		m_light_radius_arr.emplace_back(10.0f);
		m_index_entities.emplace_back(_e);
		return true;
	}

	void PointLightManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			auto iter = m_entity_map.find(_entities[i]);
			if (iter != m_entity_map.end())
			{
				unsigned int const swap_index_to_back = iter->second;
				std::swap(m_light_color_arr[swap_index_to_back], m_light_color_arr.back());
				std::swap(m_light_radius_arr[swap_index_to_back], m_light_radius_arr.back());
				std::swap(m_index_entities[swap_index_to_back], m_index_entities.back());
				m_entity_map[m_index_entities[swap_index_to_back]] = swap_index_to_back;
				m_light_color_arr.pop_back();
				m_light_radius_arr.pop_back();
				m_index_entities.pop_back();
				m_entity_map.erase(iter);
			}
		}
	}

	bool PointLightManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_entity_map.find(_entity) != m_entity_map.end();
	}

	void PointLightManager::impl_edit_component(Entity _entity)
	{
		unsigned int const edit_index = m_entity_map.at(_entity);
		ImGui::ColorEdit3("Color", &m_light_color_arr[edit_index].x);
		ImGui::DragFloat("Radius", &m_light_radius_arr[edit_index], 1.0f, 0.1, 10000.0f, "%.1f");
	}

	void PointLightManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
		unsigned int const entity_index = m_entity_map.at(_e);
		m_light_color_arr[entity_index] = _json_comp["color"];
		m_light_radius_arr[entity_index] = _json_comp["radius"];
	}

	PointLightManager::Collection PointLightManager::GetPointLightCollection() const
	{
		Collection new_collection;
		new_collection.m_light_count = m_light_color_arr.size();
		if (!m_light_color_arr.empty())
		{
			new_collection.m_light_color_arr = &m_light_color_arr[0];
			new_collection.m_light_radius_arr = &m_light_radius_arr[0];
			new_collection.m_light_pos_arr.reserve(m_entity_map.size());
			for (auto entity : m_index_entities)
				new_collection.m_light_pos_arr.push_back(entity.GetComponent<Component::Transform>().ComputeWorldTransform().position);
		}
		else
		{
			new_collection.m_light_color_arr = nullptr;
			new_collection.m_light_radius_arr = nullptr;
			new_collection.m_light_pos_arr.clear();
		}
		return new_collection;
	}

	void PointLightManager::impl_clear()
	{
		m_entity_map.clear();
		m_light_color_arr.clear();
		m_light_radius_arr.clear();
		m_index_entities.clear();
	}

	//////////////////////////////////////////////////////////////////
	//					Directional Light Manager
	//////////////////////////////////////////////////////////////////

	/*
	* Updates CSM textures and framebuffers to respect new texture size and partition count.
	*/
	void DirectionalLightManager::setup_csm(uint8_t _pow_two_csm_texture_size, uint8_t _partition_count)
	{
		assert(_partition_count != 0);
		auto & res_mgr = Singleton<Engine::Graphics::ResourceManager>();


		using texture_parameters = Engine::Graphics::ResourceManager::texture_parameters;
		texture_parameters csm_params;
		csm_params.m_wrap_s = GL_CLAMP_TO_EDGE;
		csm_params.m_wrap_t = GL_CLAMP_TO_EDGE;
		csm_params.m_wrap_r = GL_CLAMP_TO_EDGE;
		csm_params.m_min_filter = GL_LINEAR;
		csm_params.m_mag_filter = GL_LINEAR;

		// Make sure furthest CSM texture always has minimum size of 1x1.
		m_pow2_csm_resolution = _pow_two_csm_texture_size;
		if (m_pow2_csm_resolution < _partition_count - 1)
			m_pow2_csm_resolution = _partition_count - 1;

		for (unsigned int i = 0; i < CSM_PARTITION_COUNT; ++i)
		{
			res_mgr.DeleteTexture(m_cascade_shadow_map_textures[i]);
			res_mgr.DeleteFramebuffer(m_cascade_shadow_map_framebuffers[i]);
			m_cascade_shadow_map_textures[i] = 0;
			m_cascade_shadow_map_framebuffers[i] = 0;
		}

		GLenum const fb_attachment_points[] = { GL_NONE};
		// Rather than creating a shadow map texture for each cascade, create a single
		// shadow map texture with multiple mip-map levels.
		// This way, we only have to pass a single texture to the shader 
		// + a uniform to indicate how many cascades we are using.
		for (unsigned int i = 0; i < CSM_PARTITION_COUNT; ++i)
		{
			m_cascade_shadow_map_textures[i] = res_mgr.CreateTexture(GL_TEXTURE_2D, "CSM Texture");
			res_mgr.AllocateTextureStorage2D(
				m_cascade_shadow_map_textures[i], GL_DEPTH_COMPONENT32F, glm::uvec2(1 << (m_pow2_csm_resolution - i)),
				csm_params, CSM_PARTITION_COUNT
			);

			framebuffer_handle new_fb = res_mgr.CreateFramebuffer();

			res_mgr.BindFramebuffer(new_fb);
			res_mgr.AttachTextureToFramebuffer(new_fb, GL_DEPTH_ATTACHMENT, m_cascade_shadow_map_textures[i]);
			res_mgr.DrawFramebuffers(new_fb, sizeof(fb_attachment_points)/sizeof(GLenum), fb_attachment_points);

			m_cascade_shadow_map_framebuffers[i]= new_fb;
		}
	}

	void DirectionalLightManager::impl_clear()
	{
		m_directional_light_entity = Entity::InvalidEntity;
		auto & res_mgr = Singleton<Engine::Graphics::ResourceManager>();
		for (unsigned int i = 0; i < CSM_PARTITION_COUNT; ++i)
		{
			res_mgr.DeleteTexture(m_cascade_shadow_map_textures[i]);
			res_mgr.DeleteFramebuffer(m_cascade_shadow_map_framebuffers[i]);

			m_cascade_shadow_map_textures[i] = 0;
			m_cascade_shadow_map_framebuffers[i] = 0;
		}
	}

	bool DirectionalLightManager::impl_create(Entity _e)
	{
		if (m_directional_light_entity.Alive())
			return false;
		m_directional_light_entity = _e;
		m_light_color = glm::vec3(1.0f);
		m_partition_linearity = 0.6f;
		m_occluder_distance = 256.0f;
		m_shadow_factor = 0.5f;
		m_pow2_csm_resolution = 12;
		for (unsigned int i = 0; i < CSM_PARTITION_COUNT; ++i)
			m_cascade_shadow_bias[i] = 0.001f;

		setup_csm(m_pow2_csm_resolution, CSM_PARTITION_COUNT);

		return true;
	}

	void DirectionalLightManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			if (impl_component_owned_by_entity(_entities[i]))
			{
				impl_clear();
				return;
			}
		}
	}

	bool DirectionalLightManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_directional_light_entity == _entity;
	}

	void DirectionalLightManager::impl_edit_component(Entity _entity)
	{
		static int s_edit_csm_partition = 0;
		static bool s_view_csm = true;
		if (ImGui::IsWindowAppearing())
			s_edit_csm_partition = 0;

		ImGui::SliderFloat("Shadow Intensity", &m_shadow_factor, 0.0f, 1.0f);
		ImGui::ColorEdit3("Color", &m_light_color.x);
		ImGui::SliderFloat("Blend Distance", &m_blend_distance, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderInt("PCF Neighbour Count", &m_pcf_neighbour_count, 0, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Checkbox("Render Debug Cascades", &m_render_cascades);

		int pow2_resolution = m_pow2_csm_resolution;
		bool do_setup_csm = false;
		ImGui::SliderFloat("CSM Partition Linearity", &m_partition_linearity, 0.0f, 1.0f);
		ImGui::SliderFloat("CSM Occluder Distance", &m_occluder_distance, 0.0f, 1000.0f);
		do_setup_csm |= ImGui::SliderInt("CSM Pow2 Resolution", &pow2_resolution, CSM_PARTITION_COUNT - 1, 14, "%d", ImGuiSliderFlags_AlwaysClamp);

		ImGui::SliderInt("Edit CSM Partition", &s_edit_csm_partition, 0, CSM_PARTITION_COUNT - 1, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Partition Shadow Bias", &m_cascade_shadow_bias[s_edit_csm_partition], 0.0000001f, 0.0f, 0.01f, "%.5f");
		ImGui::Checkbox("View Partition Shadow Map", &s_view_csm);
		if (s_view_csm)
		{
			auto const csm_tex_info = Singleton<Engine::Graphics::ResourceManager>().GetTextureInfo(m_cascade_shadow_map_textures[s_edit_csm_partition]);
			float const ar = (float)csm_tex_info.m_size.x / (float)csm_tex_info.m_size.y;

			ImVec2 const avail_size = ImGui::GetContentRegionAvail();
			ImVec2 const display_size(avail_size.x, avail_size.x / ar);

			ImGui::Text("Size: %d", csm_tex_info.m_size.x);
			ImGui::Image(
				reinterpret_cast<ImTextureID>((size_t)csm_tex_info.m_gl_source_id), 
				display_size, ImVec2(0, 1), ImVec2(1, 0)
			);
		}



		if (do_setup_csm)
			setup_csm(pow2_resolution, CSM_PARTITION_COUNT);


	}

	void DirectionalLightManager::impl_deserialise_component(Entity _e, nlohmann::json const& _json_comp, Engine::Serialisation::SceneContext const* _context)
	{
	}





	//////////////////////////////////////////////////////////////////
	//					Light Components
	//////////////////////////////////////////////////////////////////



	float PointLight::GetRadius() const
	{
		return GetManager().m_light_radius_arr[get_light_index()];
	}

	void PointLight::SetRadius(float _radius)
	{
		GetManager().m_light_radius_arr[get_light_index()] = _radius;
	}

	glm::vec3 PointLight::GetColor() const
	{
		return GetManager().m_light_color_arr[get_light_index()];
	}

	void PointLight::SetColor(glm::vec3 _color)
	{
		GetManager().m_light_color_arr[get_light_index()] = _color;
	}

	size_t PointLight::get_light_index() const
	{
		return GetManager().m_entity_map.at(m_owner);
	}

	float DirectionalLight::GetLinearity() const
	{
		return GetManager().m_partition_linearity;
	}

	float DirectionalLight::GetOccluderDistance() const
	{
		return GetManager().m_occluder_distance;
	}

	void DirectionalLight::SetOccluderDistance(float _distance)
	{
		GetManager().m_occluder_distance = _distance;
	}

	glm::vec3 DirectionalLight::GetColor() const
	{
		return GetManager().m_light_color;
	}

	void DirectionalLight::SetColor(glm::vec3 _color)
	{
		GetManager().m_light_color = _color;
	}

	float DirectionalLight::GetShadowIntensity() const
	{
		return GetManager().m_shadow_factor;
	}

	void DirectionalLight::SetShadowIntensity(float _intensity)
	{
		GetManager().m_shadow_factor = std::clamp(_intensity, 0.0f, 1.0f);
	}

	float DirectionalLight::GetBlendDistance() const
	{
		return GetManager().m_blend_distance;
	}

	void DirectionalLight::SetBlendDistance(float _value)
	{
		GetManager().m_blend_distance = std::clamp(_value, 0.0f, std::numeric_limits<float>::max());
	}

	constexpr uint8_t DirectionalLight::GetPartitionCount() const
	{
		return DirectionalLightManager::CSM_PARTITION_COUNT;
	}

	float DirectionalLight::GetPartitionMinDepth(uint8_t _partition, float _near, float _far) const
	{
		float const linearity = GetLinearity();
		uint8_t const partition_count = GetPartitionCount();
		float const partition_frac = (float)_partition / (float)partition_count;
		return (1.0f - linearity) * (_near * glm::pow(_far / _near, partition_frac))
			+ linearity * (_near + partition_frac * (_far - _near));
	}

	texture_handle DirectionalLight::GetPartitionShadowMapTexture(uint8_t _partition) const
	{
		return GetManager().m_cascade_shadow_map_textures[_partition];
	}

	framebuffer_handle DirectionalLight::GetPartitionFrameBuffer(uint8_t _partition) const
	{
		return GetManager().m_cascade_shadow_map_framebuffers[_partition];
	}

	float DirectionalLight::GetPartitionBias(uint8_t _partition) const
	{
 		return GetManager().m_cascade_shadow_bias[_partition];
	}

	void DirectionalLight::SetPartitionBias(uint8_t _partition, float _bias)
	{
		GetManager().m_cascade_shadow_bias[_partition] = _bias;
	}

	bool DirectionalLight::GetCascadeDebugRendering() const
	{
		return GetManager().m_render_cascades;
	}

	unsigned int DirectionalLight::GetPCFNeighbourCount() const
	{
		return GetManager().m_pcf_neighbour_count;
	}

	glm::vec3 DirectionalLight::GetLightDirection() const
	{
		auto transform_comp = Owner().GetComponent<Transform>();
		glm::vec4 vector(0.0f, 0.0f, -1.0f, 0.0f);
		return glm::normalize(glm::vec3(transform_comp.ComputeWorldMatrix() * vector));
	}


}