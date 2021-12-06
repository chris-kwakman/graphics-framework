#ifndef SANDBOX_VOLUMETRIC_FOG_H
#define SANDBOX_VOLUMETRIC_FOG_H

#include <Engine/Graphics/manager.h>
#include <Sandbox/render_common.h>
#include <Engine/Graphics/camera_data.h>

namespace Sandbox
{

	using namespace Engine::Graphics;

	// Should follow std430 GLSL alignment & packing format.
	struct volumetric_fog_instance
	{
		glm::mat4x4 m_inv_m;
	};

	struct volumetric_fog_pipeline_data
	{
		/*
		* View-frustum-aligned 3D volumetric textures.
		* X/Y dimensions are uniformly aligned with the main viewport X/Y dimensions.
		* Z dimension is uniformly aligned with screen depth, but uses non-uniform exponential
		* depth slice distribution to an artist-defined range.
		* 
		* Format: RGBA 16-bit floating point.
		*/
		glm::uvec3 m_volumetric_texture_resolution = { 160,90,128 };
		GLuint m_volumetric_density_texture = 0;
		GLuint m_volumetric_inscattering_texture = 0;

		// Data buffers that volumetric fog data will be uploaded to.
		// I.e. fog entity positions, sizes, shapes, lighting properties, etc...
		GLuint m_fog_instance_buffer = 0;

		float m_fog_cam_near = 0.1f;
		float m_fog_cam_far = 64.0f;

		shader_program_handle m_density_compute_shader;
		shader_program_handle m_inscattering_compute_shader;
	};
	extern volumetric_fog_pipeline_data s_volumetric_fog_pipeline_data;

	void set_volfog_instances(volumetric_fog_instance* _volfog_instance_data, size_t _volfog_instances);

	void setup_volumetric_fog();
	void shutdown_volumetric_fog();

	void resize_volumetric_textures(glm::uvec3 _resolution);

	void pipeline_volumetric_fog(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
	void compute_density_pass(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
	void compute_inscattering_pass(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
}

#endif // !SANDBOX_VOLUMETRIC_FOG_H
