#include "volumetric_fog.h"
#include <Demo/GraphicsPipeline/lighting_pass.h>
#include <Demo/Components/VolumetricFog.h>
#include <Engine/Components/Transform.h>
#include <Engine/Utils/singleton.h>
#include <Engine/Utils/filesystem.h>

namespace Sandbox
{
	volumetric_fog_pipeline_data s_volumetric_fog_pipeline_data{};
	size_t const MAX_VOLUMETRIC_FOG_INSTANCES = 256u;

	ubo_volfog_camera s_volfog_camera_ubo;
	ubo_volfog_shader_properties s_volfog_shader_properties;
	ssbo_volfog_instance s_volfog_instance_data[MAX_VOLUMETRIC_FOG_INSTANCES];
	size_t s_volfog_instance_count = 0;
	bool s_update_volfog_instance_data_buffer = false;
	static bool s_accumulation_texture_cleared = false;

	const GLuint BINDING_POINT_SSBO_VOLFOG_INSTANCES = 1;

	const GLenum EFORMAT_TEXTURE_DENSITY = GL_R16F;
	const GLenum EFORMAT_TEXTURE_INSCATTERING = GL_RGBA16F;
	const GLenum EFORMAT_TEXTURE_ACCUMULATION = GL_RGBA16F;


	void set_volfog_instances(ssbo_volfog_instance* _volfog_instance_data, size_t _volfog_instances)
	{
		s_update_volfog_instance_data_buffer = true;
		s_volfog_instance_count = std::min(MAX_VOLUMETRIC_FOG_INSTANCES, _volfog_instances);
		memcpy(
			s_volfog_instance_data,
			_volfog_instance_data,
			sizeof(ssbo_volfog_instance) * s_volfog_instance_count
		);
	}

	void setup_volumetric_fog()
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		std::vector<fs::path> const compute_shader_paths
		{
			"data/shaders/volfog_cs_density.comp",
			"data/shaders/volfog_cs_inscattering.comp",
			"data/shaders/volfog_cs_raymarching.comp"
		};
		res_mgr.LoadShaders(compute_shader_paths);

		// Cache shaders so we don't have to look for them later.
		pipeline_data.m_density_compute_shader = res_mgr.LoadShaderProgram(
			"volfog_cs_density", 
			{ "data/shaders/volfog_cs_density.comp" }
		);
		pipeline_data.m_inscattering_compute_shader = res_mgr.LoadShaderProgram(
			"volfog_cs_inscattering", 
			{ "data/shaders/volfog_cs_inscattering.comp" }
		);
		pipeline_data.m_raymarching_compute_shader = res_mgr.LoadShaderProgram(
			"volfog_cs_raymarching",
			{ "data/shaders/volfog_cs_raymarching.comp" }
		);

		ssbo_volfog_instance instance;
		Engine::Math::transform3D volfog_instance_transform;
		volfog_instance_transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
		volfog_instance_transform.scale = glm::vec3(20.0, 3.0f, 20.0f);
		instance.m_inv_m = volfog_instance_transform.GetInvMatrix();
		instance.m_density = 0.05f;
		instance.m_density_height_attenuation = 3.0f;

		GLuint buffers[3];
		glCreateBuffers(sizeof(buffers) / sizeof(GLuint), buffers);
		pipeline_data.m_fog_instance_ssbo = buffers[0];
		pipeline_data.m_fog_shader_properties_ubo = buffers[1];
		pipeline_data.m_fog_cam_ubo = buffers[2];

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ssbo_volfog_instance) * MAX_VOLUMETRIC_FOG_INSTANCES,
			s_volfog_instance_data, GL_DYNAMIC_DRAW
		);
		glObjectLabel(GL_BUFFER, pipeline_data.m_fog_instance_ssbo, -1, "SSBO_VOLFOG_Instances");
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_POINT_SSBO_VOLFOG_INSTANCES, pipeline_data.m_fog_instance_ssbo);

		glBindBuffer(GL_UNIFORM_BUFFER, pipeline_data.m_fog_shader_properties_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_volfog_shader_properties), &s_volfog_shader_properties, GL_STATIC_DRAW);
		glObjectLabel(GL_BUFFER, pipeline_data.m_fog_shader_properties_ubo, -1, "UBO_VOLFOG_ShaderProperties");
		glBindBufferBase(
			GL_UNIFORM_BUFFER, 
			ubo_volfog_shader_properties::BINDING_POINT,
			pipeline_data.m_fog_shader_properties_ubo
		);

		glBindBuffer(GL_UNIFORM_BUFFER, pipeline_data.m_fog_cam_ubo);
		glBufferData(
			GL_UNIFORM_BUFFER, 
			sizeof(ubo_volfog_camera), 
			&s_volfog_camera_ubo, 
			GL_DYNAMIC_DRAW
		);
		glObjectLabel(GL_BUFFER, pipeline_data.m_fog_cam_ubo, -1, "UBO_VOLFOG_FogCamera");
		glBindBufferBase(GL_UNIFORM_BUFFER, ubo_volfog_camera::BINDING_POINT, pipeline_data.m_fog_cam_ubo);

		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		resize_volumetric_textures(pipeline_data.m_volumetric_texture_resolution);

		set_volfog_instances(&instance, 1);
	}

	void shutdown_volumetric_fog()
	{
		auto& pipeline_data = s_volumetric_fog_pipeline_data;
		glDeleteBuffers(1, &pipeline_data.m_fog_instance_ssbo);

		// Texture deletion will be handled by resource manager
	}

	void resize_volumetric_textures(glm::uvec3 _resolution)
	{
		using tex_params = Engine::Graphics::ResourceManager::texture_parameters;
		tex_params volumetric_texture_params;
		volumetric_texture_params.m_wrap_s = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_wrap_t = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_wrap_r = GL_CLAMP_TO_EDGE;
		volumetric_texture_params.m_min_filter = GL_LINEAR;
		volumetric_texture_params.m_mag_filter = GL_LINEAR;

		auto & res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		pipeline_data.m_volumetric_texture_resolution = _resolution;

		if (pipeline_data.m_volumetric_density_texture != 0)
			res_mgr.DeleteTexture(pipeline_data.m_volumetric_density_texture);
		if (pipeline_data.m_volumetric_inscattering_texture != 0)
			res_mgr.DeleteTexture(pipeline_data.m_volumetric_inscattering_texture);
		if (pipeline_data.m_volumetric_accumulation_texture != 0)
			res_mgr.DeleteTexture(pipeline_data.m_volumetric_accumulation_texture);

		pipeline_data.m_volumetric_density_texture = 0;
		pipeline_data.m_volumetric_inscattering_texture = 0;
		pipeline_data.m_volumetric_accumulation_texture = 0;

		pipeline_data.m_volumetric_density_texture = res_mgr.CreateTexture(GL_TEXTURE_3D, "volumetric_fog/density");
		pipeline_data.m_volumetric_inscattering_texture = res_mgr.CreateTexture(GL_TEXTURE_3D, "volumetric_fog/in_scattering");
		pipeline_data.m_volumetric_accumulation_texture = res_mgr.CreateTexture(GL_TEXTURE_3D, "volumetric_fog/accumulation");

		res_mgr.AllocateTextureStorage3D(
			pipeline_data.m_volumetric_density_texture,
			EFORMAT_TEXTURE_DENSITY,
			pipeline_data.m_volumetric_texture_resolution,
			volumetric_texture_params
		);
		res_mgr.AllocateTextureStorage3D(
			pipeline_data.m_volumetric_inscattering_texture,
			EFORMAT_TEXTURE_INSCATTERING,
			pipeline_data.m_volumetric_texture_resolution,
			volumetric_texture_params
		);
		res_mgr.AllocateTextureStorage3D(
			pipeline_data.m_volumetric_accumulation_texture,
			EFORMAT_TEXTURE_ACCUMULATION,
			pipeline_data.m_volumetric_texture_resolution,
			volumetric_texture_params
		);
	}

	void pipeline_volumetric_fog(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _camera_data)
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		// Override camera near / far.
		s_volfog_camera_ubo.m_near = _camera_data.m_near;
		s_volfog_camera_ubo.m_far = _camera_data.m_far;
		//_camera_data.m_near = s_volfog_camera_ubo.m_near;
		//_camera_data.m_far = s_volfog_camera_ubo.m_far;

		auto& volfog_comp_mgr = Singleton<Component::VolumetricFogManager>();

		if (volfog_comp_mgr.m_data.ResetVolFogTextureSize)
		{
			volfog_comp_mgr.m_data.ResetVolFogTextureSize = false;
			resize_volumetric_textures(volfog_comp_mgr.m_data.NewVolFogTextureSize);
			s_accumulation_texture_cleared = false;
		}
		s_volfog_camera_ubo.m_layer_count = pipeline_data.m_volumetric_texture_resolution.z;

		auto const & fog_instances = volfog_comp_mgr.GetAllFogInstances();
		std::vector<ssbo_volfog_instance> upload_instances;
		upload_instances.reserve(fog_instances.size());
		for (auto const & pair : fog_instances)
		{
			auto const fog_instance = pair.second;
			ssbo_volfog_instance new_instance;
			new_instance.m_density = fog_instance.m_base_density;
			new_instance.m_density_height_attenuation = fog_instance.m_density_height_attenuation;
			new_instance.m_inv_m = pair.first.GetComponent<Component::Transform>().ComputeWorldTransform().GetInvMatrix();
			upload_instances.emplace_back(std::move(new_instance));
		}

		set_volfog_instances(
			upload_instances.empty() ? nullptr : &upload_instances.front(), upload_instances.size()
		);

		/*
		* Before doing volumetric fog pass, check if we have any Volumetric Fog instances.
		* If not, do not perform volfog pass and clear the final accumulation texture so 
		* that we can use a default accumulation texture for applying volfog effect to shaded objects.
		* This prevents rendering issues where we're trying to do a volfog pass without any fog instances.
		*/
		if (fog_instances.empty() && !s_accumulation_texture_cleared)
		{
			auto accum_tex_info = res_mgr.GetTextureInfo(pipeline_data.m_volumetric_accumulation_texture);
			glm::vec4 const clear_color{ 0.0f, 0.0f, 0.0f, 1.0f };
			glClearTexImage(accum_tex_info.m_gl_source_id, 0, GL_RGBA, GL_FLOAT, &clear_color);
			return;
		}
		else
		{
			s_accumulation_texture_cleared = false;
		}

		if (s_update_volfog_instance_data_buffer)
		{
			s_update_volfog_instance_data_buffer = false;

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_ssbo);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ssbo_volfog_instance) * s_volfog_instance_count,
				s_volfog_instance_data, GL_DYNAMIC_DRAW
			);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		s_volfog_camera_ubo.m_inv_vp = glm::inverse(
			_camera_data.get_perspective_matrix() * _cam_transform.GetInvMatrix()
		);
		s_volfog_camera_ubo.m_view_dir = _cam_transform.quaternion * glm::vec3(0.0f, 0.0f, -1.0f);
		s_volfog_camera_ubo.m_world_pos = _cam_transform.position;
		s_volfog_camera_ubo.m_layer_linearity = volfog_comp_mgr.m_data.LayerLinearity;
		
		glBindBuffer(GL_UNIFORM_BUFFER, pipeline_data.m_fog_cam_ubo);
		glBufferData(
			GL_UNIFORM_BUFFER,
			sizeof(ubo_volfog_camera),
			&s_volfog_camera_ubo,
			GL_DYNAMIC_DRAW
		);

		s_volfog_shader_properties.m_fog_albedo = volfog_comp_mgr.m_data.FogAlbedo;
		s_volfog_shader_properties.m_phase_anisotropy = volfog_comp_mgr.m_data.FogPhaseAnisotropy;
		s_volfog_shader_properties.m_scattering_coefficient = volfog_comp_mgr.m_data.ScatteringCoefficient;
		s_volfog_shader_properties.m_absorption_coefficient = volfog_comp_mgr.m_data.AbsorptionCoefficient;
		s_volfog_shader_properties.m_fog_noise_min = volfog_comp_mgr.m_data.NoiseMinValue;
		s_volfog_shader_properties.m_fog_noise_max = volfog_comp_mgr.m_data.NoiseMaxValue;
		s_volfog_shader_properties.m_fog_noise_scale = volfog_comp_mgr.m_data.NoiseScale;
		s_volfog_shader_properties.m_wind_dir = volfog_comp_mgr.m_data.WindDirection;
		s_volfog_shader_properties.m_time = float(s_time);


		glBindBuffer(GL_UNIFORM_BUFFER, pipeline_data.m_fog_shader_properties_ubo);
		glBufferData(
			GL_UNIFORM_BUFFER,
			sizeof(ubo_volfog_shader_properties),
			&s_volfog_shader_properties,
			GL_DYNAMIC_DRAW
		);

		compute_density_pass();
		compute_inscattering_pass();
		perform_raymarching_pass();
	}

	void compute_density_pass()
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto& pipeline_data = s_volumetric_fog_pipeline_data;

		ResourceManager::texture_info const density_tex_info = res_mgr.GetTextureInfo(pipeline_data.m_volumetric_density_texture);

		res_mgr.UseProgram(pipeline_data.m_density_compute_shader);

		GLuint const LOC_UBO_FOGCAM = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, "ubo_fogcam"
		);
		GLuint const LOC_SSBO_VOLFOG_INSTANCES = glGetProgramResourceIndex(
			res_mgr.m_bound_gl_program_object, GL_SHADER_STORAGE_BLOCK, "ssbo_volfog_instance_data"
		);
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_FOGCAM,
			ubo_volfog_camera::BINDING_POINT
		);
		glShaderStorageBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_SSBO_VOLFOG_INSTANCES,
			BINDING_POINT_SSBO_VOLFOG_INSTANCES
		);

		// Bind buffer containing fog instance data
		//GfxCall(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pipeline_data.m_fog_instance_buffer));
		//GLuint const volfog_instance_data_loc = glGetProgramResourceIndex(pipeline_data.m_density_compute_shader, GL_SHADER_STORAGE_BLOCK, "volfog_instance_data");
		//glShaderStorageBlockBinding(pipeline_data.m_density_compute_shader, volfog_instance_data_loc, SSBO_BUFF_IDX_VOLFOG_INSTANCES);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, pipeline_data.m_fog_instance_buffer);

		// Bind texture that we will write output data to for usage in light in-scattering pass.
		glBindImageTexture(
			0,
			density_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_WRITE_ONLY,
			EFORMAT_TEXTURE_DENSITY
		);

		// Brute force approach until we get things working.
		// TODO: Less work groups of larger size.
		glDispatchCompute(
			pipeline_data.m_volumetric_texture_resolution.x / 10,
			pipeline_data.m_volumetric_texture_resolution.y / 10,
			pipeline_data.m_volumetric_texture_resolution.z / 8
		);

	}
	void compute_inscattering_pass()
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

		GLuint const LOC_IMAGE_DENSITY = res_mgr.FindBoundProgramUniformLocation("u_in_density");
		GLuint const LOC_IMAGE_INSCATTERING = res_mgr.FindBoundProgramUniformLocation("u_out_inscattering");
		int LOC_SAMPLER_SHADOW_MAP_0 = res_mgr.FindBoundProgramUniformLocation("u_sampler_shadow_map[0]");

		int LOC_UBO_CAMERA_DATA = glGetUniformBlockIndex(res_mgr.m_bound_gl_program_object, "ubo_camera_data");
		int LOC_UBO_CSM_DATA = glGetUniformBlockIndex(res_mgr.m_bound_gl_program_object, "ubo_csm_data");
		GLuint const LOC_UBO_FOGCAM = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, "ubo_fogcam"
		);
		GLuint const LOC_UBO_SHADER_PROPERTIES = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, "ubo_shader_properties"
		);
		GLuint const LOC_SSBO_VOLFOG_INSTANCES = glGetProgramResourceIndex(
			res_mgr.m_bound_gl_program_object, GL_SHADER_STORAGE_BLOCK, "ssbo_volfog_instance_data"
		);
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_FOGCAM,
			ubo_volfog_camera::BINDING_POINT
		);
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object, 
			LOC_UBO_SHADER_PROPERTIES, 
			ubo_volfog_shader_properties::BINDING_POINT
		);
		// Bind camera data UBO
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_CAMERA_DATA,
			ubo_camera_data::BINDING_POINT
		);
		// Bind CSM data UBO
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_CSM_DATA,
			ubo_cascading_shadow_map_data::BINDING_POINT
		);
		glShaderStorageBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_SSBO_VOLFOG_INSTANCES,
			ssbo_volfog_instance::BINDING_POINT
		);

		GLuint const UNIT_IMAGE_DENSITY = 0;
		GLuint const UNIT_IMAGE_INSCATTERING = 1;

		// Bind input image (density texture)
		glBindImageTexture(
			UNIT_IMAGE_DENSITY,
			density_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_READ_ONLY,
			EFORMAT_TEXTURE_DENSITY
		);
		// Bind output image (inlight scattering)
		glBindImageTexture(
			UNIT_IMAGE_INSCATTERING,
			inscattering_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_WRITE_ONLY,
			EFORMAT_TEXTURE_INSCATTERING
		);

		if(LOC_IMAGE_DENSITY != -1)
			res_mgr.SetBoundProgramUniform(LOC_IMAGE_DENSITY, (GLint)UNIT_IMAGE_DENSITY);
		if(LOC_IMAGE_INSCATTERING != -1)
			res_mgr.SetBoundProgramUniform(LOC_IMAGE_INSCATTERING, (GLint)UNIT_IMAGE_INSCATTERING);
		if (LOC_SAMPLER_SHADOW_MAP_0 != -1)
		{
			auto const dir_light = Singleton<Component::DirectionalLightManager>().GetDirectionalLight();
			for (unsigned int i = 0; i < Component::DirectionalLightManager::CSM_PARTITION_COUNT; ++i)
			{
				activate_texture(
					dir_light.GetPartitionShadowMapTexture(i),
					LOC_SAMPLER_SHADOW_MAP_0 + i,
					i
				);
			}
		}

		// Brute force approach until we get things working.
		// TODO: Less work groups of larger size.
		glDispatchCompute(
			pipeline_data.m_volumetric_texture_resolution.x / 10,
			pipeline_data.m_volumetric_texture_resolution.y / 10,
			pipeline_data.m_volumetric_texture_resolution.z / 8
		);

	}

	void perform_raymarching_pass()
	{
		auto& res_mgr = Singleton<ResourceManager>();
		auto const& pipeline_data = s_volumetric_fog_pipeline_data;

		GLuint const UNIT_IMAGE_INSCATTERING = 0;
		GLuint const UNIT_IMAGE_ACCUMULATION = 1;

		res_mgr.UseProgram(pipeline_data.m_raymarching_compute_shader);

		ResourceManager::texture_info const inscattering_tex_info = res_mgr.GetTextureInfo(
			pipeline_data.m_volumetric_inscattering_texture
		);
		ResourceManager::texture_info const accumulation_tex_info = res_mgr.GetTextureInfo(
			pipeline_data.m_volumetric_accumulation_texture
		);

		// Bind input image (inscattering texture)
		glBindImageTexture(
			UNIT_IMAGE_INSCATTERING,
			inscattering_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_READ_ONLY,
			EFORMAT_TEXTURE_INSCATTERING
		);
		// Bind output image (accumulation texture)
		glBindImageTexture(
			UNIT_IMAGE_ACCUMULATION,
			accumulation_tex_info.m_gl_source_id,
			0,
			GL_TRUE,
			0,
			GL_WRITE_ONLY,
			EFORMAT_TEXTURE_ACCUMULATION
		);

		GLuint const LOC_IMAGE_INSCATTERING = res_mgr.FindBoundProgramUniformLocation("u_in_inscattering");
		GLuint const LOC_IMAGE_ACCUMULATION = res_mgr.FindBoundProgramUniformLocation("u_out_accumulation");
		if (LOC_IMAGE_INSCATTERING != -1)
			res_mgr.SetBoundProgramUniform(LOC_IMAGE_INSCATTERING, (GLint)UNIT_IMAGE_INSCATTERING);
		if (LOC_IMAGE_ACCUMULATION != -1)
			res_mgr.SetBoundProgramUniform(LOC_IMAGE_ACCUMULATION, (GLint)UNIT_IMAGE_ACCUMULATION);

		// Only a single layer of groups that traverses the texture serially slice-by-slice.
		glDispatchCompute(
			pipeline_data.m_volumetric_texture_resolution.x / 10,
			pipeline_data.m_volumetric_texture_resolution.y / 10,
			1
		);
	}
}