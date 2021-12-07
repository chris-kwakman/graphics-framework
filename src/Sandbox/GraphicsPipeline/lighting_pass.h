#ifndef SANDBOX_LIGHTING_PASS_H
#define SANDBOX_LIGHTING_PASS_H

#include <glm/vec3.hpp>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera_data.h>
#include <Engine/Components/Light.h>

namespace Sandbox {

	using texture_handle = Engine::Graphics::texture_handle;
	using mesh_handle = Engine::Graphics::mesh_handle;
	using framebuffer_handle = Engine::Graphics::framebuffer_handle;

	struct ubo_cascading_shadow_map_data
	{
		static GLuint const BINDING_POINT_UBO_CSM_DATA = 8;
		glm::mat4x4 m_light_transformations[Component::DirectionalLightManager::CSM_PARTITION_COUNT];
		glm::vec3	m_world_light_dir;
		float		m_shadow_bias[Component::DirectionalLightManager::CSM_PARTITION_COUNT];
		float		m_cascade_clipspace_end[Component::DirectionalLightManager::CSM_PARTITION_COUNT];
		float		m_cascade_blend_clipspace_start[Component::DirectionalLightManager::CSM_PARTITION_COUNT - 1];
		float		m_shadow_intensity = 0.5f;
		unsigned int m_pcf_neighbour_count = 2;
	};

	struct lighting_pass_pipeline_data
	{
		GLuint ubo_csm;
	};

	extern lighting_pass_pipeline_data s_lighting_pass_pipeline_data;

	void setup_lighting_pass_pipeline();
	void shutdown_lighting_pass_pipeline();

	void RenderPointLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	);

	ubo_cascading_shadow_map_data RenderDirectionalLightCSM(
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	);

	void RenderShadowMapToFrameBuffer(
		Engine::ECS::Entity _camera_entity, 
		glm::uvec2 _viewport_size, 
		ubo_cascading_shadow_map_data _csm_data,
		texture_handle	  _depth_texture,
		texture_handle	  _normal_texture,
		framebuffer_handle _shadow_frame_buffer
	);
}



#endif // !SANDBOX_LIGHTING_PASS_H
