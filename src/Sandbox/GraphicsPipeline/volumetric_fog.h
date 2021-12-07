#ifndef SANDBOX_VOLUMETRIC_FOG_H
#define SANDBOX_VOLUMETRIC_FOG_H

#include <Engine/Graphics/manager.h>
#include <Sandbox/render_common.h>
#include <Engine/Graphics/camera_data.h>

namespace Sandbox
{

	using namespace Engine::Graphics;

	// Should follow std430 GLSL alignment & packing format.
	struct ssbo_volfog_instance
	{
		glm::mat4x4		m_inv_m;
		float			m_density;
		float			m_density_height_attenuation;
		float			_padding[2];
	};

	struct ubo_volfog_shader_properties
	{
		// Fog rendering properties
		glm::vec3	m_fog_albedo = glm::vec3(1.0f);
		float		_padding;
		float		m_scattering_coefficient;
		float		m_absorption_coefficient;
		float		m_phase_anisotropy;
	};

	struct ubo_volfog_camera
	{
		// Do not reorder
		glm::mat4x4 m_inv_vp;
		glm::vec3	m_view_dir;
		float		m_near = 0.1f;
		glm::vec3	m_world_pos;
		float		m_far = 100.0f;
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
		GLuint m_fog_instance_ssbo = 0;	// SSBO
		GLuint m_fog_shader_properties_ubo = 0; // UBO
		GLuint m_fog_cam_ubo = 0;

		shader_program_handle m_density_compute_shader;
		shader_program_handle m_inscattering_compute_shader;
	};
	extern volumetric_fog_pipeline_data s_volumetric_fog_pipeline_data;

	void set_volfog_instances(ssbo_volfog_instance* _volfog_instance_data, size_t _volfog_instances);

	void setup_volumetric_fog();
	void shutdown_volumetric_fog();

	void resize_volumetric_textures(glm::uvec3 _resolution);

	void pipeline_volumetric_fog(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
	void compute_density_pass(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
	void compute_inscattering_pass(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data);
}

#endif // !SANDBOX_VOLUMETRIC_FOG_H
