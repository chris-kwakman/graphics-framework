#include "volumetric_fog.h"
#include <Engine/Utils/singleton.h>

namespace Sandbox
{
	volumetric_fog_pipeline_data s_volumetric_fog_pipeline_data{};
	size_t const MAX_VOLUMETRIC_FOG_INSTANCES = 256u;

	volumetric_fog_instance s_volfog_instance_data[MAX_VOLUMETRIC_FOG_INSTANCES];
	size_t s_volfog_instance_count = 0;
	bool s_update_volfog_instance_data_buffer = false;

	const GLuint SSBO_BUFF_IDX_VOLFOG_INSTANCES = 0;
	const GLuint BINDING_POINT_SSBO_VOLFOG_INSTANCES = 1;

	void set_volfog_instances(volumetric_fog_instance* _volfog_instance_data, size_t _volfog_instances)
	{
		s_update_volfog_instance_data_buffer = true;
		s_volfog_instance_count = std::min(MAX_VOLUMETRIC_FOG_INSTANCES, _volfog_instances);
		memcpy(
			s_volfog_instance_data,
			_volfog_instance_data,
			sizeof(volumetric_fog_instance) * s_volfog_instance_count
		);
	}

	void setup_volumetric_fog()
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		std::vector<std::filesystem::path> const compute_shader_paths
		{
			"data/shaders/volfog_cs_density.comp",
			//"volfog_cs_inscattering.comp"
		};
		res_mgr.LoadShaders(compute_shader_paths);

		// Cache shaders so we don't have to look for them later.
		pipeline_data.m_density_compute_shader = res_mgr.LoadShaderProgram(
			"volfog_cs_density", 
			{ "data/shaders/volfog_cs_density.comp" }
		);
		//pipeline_data.m_inscattering_compute_shader = res_mgr.LoadShaderProgram(
		//	"volfog_cs_inscattering", 
		//	{ "volfog_cs_inscattering.comp" }
		//);
		res_mgr.UseProgram(pipeline_data.m_density_compute_shader);
		
		int LOC_VOLFOG_INSTANCE_DATA = glGetProgramResourceIndex(res_mgr.m_bound_gl_program_object, GL_SHADER_STORAGE_BLOCK, "volfog_instance_data");
		glShaderStorageBlockBinding(
			res_mgr.m_bound_gl_program_object, 
			LOC_VOLFOG_INSTANCE_DATA, 
			BINDING_POINT_SSBO_VOLFOG_INSTANCES
		);

		volumetric_fog_instance instance;
		Engine::Math::transform3D volfog_instance_transform;
		volfog_instance_transform.position = glm::vec3(0.0f);
		volfog_instance_transform.scale = glm::vec3(4.0, 1.0f, 4.0f);
		instance.m_inv_m = volfog_instance_transform.GetInvMatrix();

		glCreateBuffers(1, &pipeline_data.m_fog_instance_buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(volumetric_fog_instance) * MAX_VOLUMETRIC_FOG_INSTANCES,
			s_volfog_instance_data, GL_DYNAMIC_DRAW
		);
		glObjectLabel(GL_BUFFER, pipeline_data.m_fog_instance_buffer, -1, "Buffer_VolumetricFogInstances");
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_POINT_SSBO_VOLFOG_INSTANCES, pipeline_data.m_fog_instance_buffer);

		resize_volumetric_textures(pipeline_data.m_volumetric_texture_resolution);

		set_volfog_instances(&instance, 1);
	}

	void shutdown_volumetric_fog()
	{
		auto& pipeline_data = s_volumetric_fog_pipeline_data;
		glDeleteBuffers(1, &pipeline_data.m_fog_instance_buffer);

		// Texture deletion will be handled by resource manager
	}

	void resize_volumetric_textures(glm::uvec3 _resolution)
	{
		using tex_params = Engine::Graphics::ResourceManager::texture_parameters;
		tex_params volumetric_texture_params;
		volumetric_texture_params.m_wrap_s = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_wrap_r = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_wrap_t = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_min_filter = GL_LINEAR;
		volumetric_texture_params.m_mag_filter = GL_LINEAR;

		auto & res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		pipeline_data.m_volumetric_texture_resolution = _resolution;

		if(pipeline_data.m_volumetric_density_texture == 0)
			pipeline_data.m_volumetric_density_texture = res_mgr.CreateTexture(GL_TEXTURE_3D, "volumetric_fog/density");
		if(pipeline_data.m_volumetric_inscattering_texture == 0)
			pipeline_data.m_volumetric_inscattering_texture = res_mgr.CreateTexture(GL_TEXTURE_3D, "volumetric_fog/in_scattering");

		res_mgr.AllocateTextureStorage3D(
			pipeline_data.m_volumetric_density_texture,
			GL_RGBA16F, 
			pipeline_data.m_volumetric_texture_resolution,
			volumetric_texture_params
		);
		res_mgr.AllocateTextureStorage3D(
			pipeline_data.m_volumetric_inscattering_texture,
			GL_RGBA16F,
			pipeline_data.m_volumetric_texture_resolution,
			volumetric_texture_params
		);
	}

	void pipeline_volumetric_fog(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data)
	{
		// Override camera near / far.
		_camera_data.m_near = s_volumetric_fog_pipeline_data.m_fog_cam_near;
		_camera_data.m_far = s_volumetric_fog_pipeline_data.m_fog_cam_far;

		auto& pipeline_data = s_volumetric_fog_pipeline_data;
		if (s_update_volfog_instance_data_buffer)
		{
			s_update_volfog_instance_data_buffer = false;

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_buffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(volumetric_fog_instance) * s_volfog_instance_count,
				s_volfog_instance_data, GL_DYNAMIC_DRAW
			);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		compute_density_pass(_cam_transform, _camera_data);
		//compute_inscattering_pass();
	}

	void compute_density_pass(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data)
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		ResourceManager::texture_info const density_tex_info = res_mgr.GetTextureInfo(pipeline_data.m_volumetric_density_texture);

		res_mgr.UseProgram(pipeline_data.m_density_compute_shader);
		int LOC_MAT_FOG_CAM_INV_VP = res_mgr.FindBoundProgramUniformLocation("u_fog_cam_inv_vp");
		int LOC_VOLFOG_VOLUME_TEX_SIZE = res_mgr.FindBoundProgramUniformLocation("u_volfog_volume_texture_size");
		int LOC_FOG_CAM_NEAR = res_mgr.FindBoundProgramUniformLocation("u_fog_cam_near");
		int LOC_FOG_CAM_FAR = res_mgr.FindBoundProgramUniformLocation("u_fog_cam_far");
		res_mgr.SetBoundProgramUniform(LOC_MAT_FOG_CAM_INV_VP, 
			glm::inverse(_camera_data.get_perspective_matrix() * _cam_transform.GetInvMatrix())
		);
		res_mgr.SetBoundProgramUniform(LOC_VOLFOG_VOLUME_TEX_SIZE, s_volumetric_fog_pipeline_data.m_volumetric_texture_resolution);

		// Bind buffer containing fog instance data
		//GfxCall(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pipeline_data.m_fog_instance_buffer));
		//GLuint const volfog_instance_data_loc = glGetProgramResourceIndex(pipeline_data.m_density_compute_shader, GL_SHADER_STORAGE_BLOCK, "volfog_instance_data");
		//glShaderStorageBlockBinding(pipeline_data.m_density_compute_shader, volfog_instance_data_loc, SSBO_BUFF_IDX_VOLFOG_INSTANCES);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_buffer);

		// Bind texture that we will write output data to for usage in light in-scattering pass.
		GfxCall(glBindImageTexture(
			0,
			density_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_WRITE_ONLY,
			GL_RGBA16F
		));

		// Brute force approach until we get things working.
		// TODO: Less work groups of larger size.
		GfxCall(glDispatchCompute(
			pipeline_data.m_volumetric_texture_resolution.x / 10,
			pipeline_data.m_volumetric_texture_resolution.y / 10,
			pipeline_data.m_volumetric_texture_resolution.z / 8
		));

	}
	void compute_inscattering_pass(glm::mat4 const& _cam_inv_vp)
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		ResourceManager::texture_info const density_tex_info = res_mgr.GetTextureInfo(
			pipeline_data.m_volumetric_density_texture
		);
		ResourceManager::texture_info const inscattering_tex_info = res_mgr.GetTextureInfo(
			pipeline_data.m_volumetric_inscattering_texture
		);

		res_mgr.UseProgram(pipeline_data.m_inscattering_compute_shader);

		// Bind input image (density texture)
		GfxCall(glBindImageTexture(
			0,
			density_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_READ_ONLY,
			GL_RGBA16F
		));
		// Bind output image (inlight scattering)
		GfxCall(glBindImageTexture(
			1,
			density_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_WRITE_ONLY,
			GL_RGBA16F
		));

		// Brute force approach until we get things working.
		// TODO: Less work groups of larger size.
		GfxCall(glDispatchCompute(
			pipeline_data.m_volumetric_texture_resolution.x,
			pipeline_data.m_volumetric_texture_resolution.y,
			pipeline_data.m_volumetric_texture_resolution.z
		));
	}
}