#ifndef SANDBOX_LIGHTING_PASS_H
#define SANDBOX_LIGHTING_PASS_H

#include <glm/vec3.hpp>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera_data.h>
#include <Engine/Components/Light.h>

namespace Sandbox {

	using texture_handle = Engine::Graphics::ResourceManager::texture_handle;
	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;
	using framebuffer_handle = Engine::Graphics::ResourceManager::framebuffer_handle;

	struct cascading_shadow_map_data
	{
		glm::mat4x4 m_light_transformations[Component::DirectionalLightManager::CSM_PARTITION_COUNT];
		float		m_cascade_clipspace_end[Component::DirectionalLightManager::CSM_PARTITION_COUNT];
		float		m_cascade_blend_clipspace_start[Component::DirectionalLightManager::CSM_PARTITION_COUNT - 1];
	};

	void RenderPointLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	);

	cascading_shadow_map_data RenderDirectionalLightCSM(
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	);

	void RenderShadowMapToFrameBuffer(
		Engine::ECS::Entity _camera_entity, 
		glm::uvec2 _viewport_size, 
		cascading_shadow_map_data const & _csm_data,
		texture_handle	  _depth_texture,
		texture_handle	  _normal_texture,
		framebuffer_handle _shadow_frame_buffer
	);
}



#endif // !SANDBOX_LIGHTING_PASS_H
