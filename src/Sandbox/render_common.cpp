
#include "render_common.h"
#include <Engine/Utils/singleton.h>
#include <Engine/Graphics/manager.h>

namespace Sandbox
{

	// Graphics Objects

	texture_handle
		s_fb_texture_depth,
		s_fb_texture_base_color,
		s_fb_texture_normal,
		s_fb_texture_metallic_roughness,
		s_fb_texture_light_color,
		s_fb_texture_luminance,
		s_fb_texture_bloom_pingpong[2],
		s_fb_texture_shadow,
		s_fb_texture_ao,
		s_fb_texture_ao_pingpong
	;

	texture_handle s_texture_white = 0;

	framebuffer_handle 
		s_framebuffer_gbuffer, 
		s_framebuffer_gbuffer_decal, 
		s_framebuffer_lighting, 
		s_framebuffer_bloom[2], 
		s_framebuffer_shadow,
		s_framebuffer_ao,
		s_framebuffer_ao_pingpong
	;

	// Lighting data
	glm::vec4		s_clear_color{ 0.0f,0.0f,0.0f,0.0f };
	glm::vec3		s_ambient_color = glm::vec3(0.005f);
	float			s_exposure = 1.1f;
	float			s_gamma_correction_factor = 1.8f;
	float			s_shininess_mult_factor = 25.0f;
	// Blur data
	bool			s_bloom_enabled = true;
	glm::uvec2		s_bloom_texture_size(0, 0);
	glm::vec3		s_bloom_treshhold_color(0.2126f, 0.7152f, 0.0722f);
	unsigned int	s_bloom_blur_count = 5;

	// Editor Data
	bool			s_render_infinite_grid = true;


	GfxAmbientOcclusion s_ambient_occlusion;

	GLuint s_buffers[1];
	GLuint s_ubo_camera = 0;

	unsigned int s_gl_tri_ibo = 0, s_gl_tri_vao = 0, s_gl_tri_vbo = 0;
	unsigned int s_gl_bone_vao, s_gl_bone_vbo, s_gl_bone_ibo, s_gl_joint_vao, s_gl_joint_vbo, s_gl_joint_ibo;
	unsigned int joint_index_count, bone_index_count;

	unsigned int s_gl_line_vao, s_gl_line_vbo, s_gl_line_ibo;


	void setup_render_common()
	{
		create_skeleton_bone_model();
		create_line_mesh();

		glGenBuffers(1, s_buffers);
		s_ubo_camera = s_buffers[0];
		glBindBuffer(GL_UNIFORM_BUFFER, s_ubo_camera);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_camera_data), nullptr, GL_DYNAMIC_DRAW);
		glObjectLabel(GL_BUFFER, s_ubo_camera, -1, "UBO_CameraData");
		glBindBufferBase(GL_UNIFORM_BUFFER, ubo_camera_data::BINDING_POINT, s_ubo_camera);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void shutdown_render_common()
	{
		glDeleteBuffers(sizeof(s_buffers) / sizeof(GLuint), s_buffers);
	}

	void update_camera_ubo(ubo_camera_data _camera_data)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, s_ubo_camera);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_camera_data), &_camera_data, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index)
	{
		// If no texture handle exists, ignore
		if (_texture == 0)
			return;

		auto & res_mgr = Singleton<Engine::Graphics::ResourceManager>();
		auto const texture_info = res_mgr.GetTextureInfo(_texture);
		GfxCall(glActiveTexture(GL_TEXTURE0 + _texture_index));
		GfxCall(glBindTexture(texture_info.m_target, texture_info.m_gl_source_id));

		res_mgr.SetBoundProgramUniform(_program_uniform_index, (int)_texture_index);
	}

	void render_primitive(mesh_primitive_data const& _primitive)
	{
		auto const & res_mgr = Singleton<Engine::Graphics::ResourceManager>();
		GfxCall(glBindVertexArray(_primitive.m_vao_gl_id));
		if (_primitive.m_index_buffer_handle != 0)
		{
			auto index_buffer_info = res_mgr.GetBufferInfo(_primitive.m_index_buffer_handle);
			auto ibo_info = res_mgr.GetIndexBufferInfo(_primitive.m_index_buffer_handle);
			GfxCall(glDrawElements(
				_primitive.m_render_mode,
				(GLsizei)_primitive.m_index_count,
				ibo_info.m_type,
				(GLvoid*)_primitive.m_index_byte_offset
			));
		}
		else
		{
			GfxCall(glDrawArrays(_primitive.m_render_mode, 0, (GLsizei)_primitive.m_vertex_count));
		}
		glBindVertexArray(0);
	}

# define M_PI           3.14159265358979323846  

	glm::vec3 spherical_to_certesian(float _h_rad, float _v_rad, float _radius = 0.5f)
	{
		return {
			_radius * cos(_v_rad) * sin(_h_rad),
			_radius * sin(_v_rad),
			_radius * cos(_v_rad) * cos(_h_rad)
		};
	}

	float vrad_to_uv(float _vrad)
	{
		const float f_PI = static_cast<float>(M_PI);
		//return (_vrad / f_PI) + 0.5f;
		return (sinf(_vrad) + 1.0f) * 0.5f;
	}

	void create_skeleton_bone_model()
	{
		GfxCall(glCreateVertexArrays(1, &s_gl_bone_vao));
		GfxCall(glCreateVertexArrays(1, &s_gl_joint_vao));
		GfxCall(glCreateBuffers(1, &s_gl_bone_vbo));
		GfxCall(glCreateBuffers(1, &s_gl_joint_vbo));
		GfxCall(glCreateBuffers(1, &s_gl_bone_ibo));
		GfxCall(glCreateBuffers(1, &s_gl_joint_ibo));

		char const * bone_vao_name = "BoneVAO";
		char const * bone_vbo_name = "BoneVBO";
		char const * bone_ibo_name = "BoneIBO";
		char const * joint_vao_name = "JointVAO";
		char const * joint_vbo_name = "JointVBO";
		char const * joint_ibo_name = "JointIBO";

		GfxCall(glObjectLabel(GL_VERTEX_ARRAY, s_gl_bone_vao, -1, bone_vao_name));
		GfxCall(glObjectLabel(GL_VERTEX_ARRAY, s_gl_joint_vao, -1, joint_vao_name));
		GfxCall(glObjectLabel(GL_BUFFER, s_gl_bone_vbo, -1, bone_vbo_name));
		GfxCall(glObjectLabel(GL_BUFFER, s_gl_joint_vbo, -1, joint_vbo_name));
		GfxCall(glObjectLabel(GL_BUFFER, s_gl_bone_ibo, -1, bone_ibo_name));
		GfxCall(glObjectLabel(GL_BUFFER, s_gl_joint_ibo, -1, joint_ibo_name));
		
		// Create joint mesh
		{
			glm::vec3 vertices[] = {
				{-0.5f, -0.5f, -0.5f},
				{-0.5f, -0.5f, 0.5f},
				{0.5f, -0.5f, 0.5f},
				{0.5f, -0.5f, -0.5f},
				{-0.5f, 0.5f, -0.5f},
				{-0.5f, 0.5f, 0.5f},
				{0.5f, 0.5f, 0.5f},
				{0.5f, 0.5f, -0.5f}
			};
			glm::uvec3 indices[] = {
				{2, 1, 0},
				{1, 3, 2},
				{4, 5, 6},
				{4, 6, 7},
				{0, 1, 5},
				{0, 5, 4},
				{1, 2, 6},
				{1, 6, 5},
				{2, 3, 7},
				{2, 7, 6},
				{3, 0, 4},
				{3, 4, 7}
			};

			joint_index_count = sizeof(indices) / sizeof(unsigned int);

			GfxCall(glBindVertexArray(s_gl_joint_vao));
			GfxCall(glBindBuffer(GL_ARRAY_BUFFER, s_gl_joint_vbo));
			GfxCall(glBufferData(
				GL_ARRAY_BUFFER,
				sizeof(vertices),
				vertices,
				GL_STATIC_DRAW
			));
			GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_joint_ibo));
			GfxCall(glBufferData(
				GL_ELEMENT_ARRAY_BUFFER,
				sizeof(indices),
				indices,
				GL_STATIC_DRAW
			));
			
			GfxCall(glEnableVertexArrayAttrib(s_gl_joint_vao, 0));

			GfxCall(glVertexAttribPointer(
				0, 3, GL_FLOAT, false, 0, (void const*)0
			));
			GfxCall(glBindVertexArray(0));
		}

		// Create bone mesh
		{
			glm::vec3 vertices[]{
				{0.0f, 0.0f, -1.0f},
				{-0.25f, -0.25f, -0.75f},
				{-0.25f, 0.25f, -0.75f},
				{0.25f, 0.25f, -0.75f},
				{0.25f, -0.25f, -0.75f},
				{0.0f, 0.0f, 0.0f}
			};
			glm::uvec3 indices[]{
				{0, 1, 2},
				{0, 2, 3},
				{0, 3, 4},
				{0, 4, 1},
				{5, 1, 4},
				{5, 4, 3},
				{5, 3, 2},
				{5, 2, 1}
			};

			bone_index_count = sizeof(indices) / sizeof(unsigned int);

			GfxCall(glBindVertexArray(s_gl_bone_vao));
			GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_bone_ibo));
			GfxCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (GLvoid*)indices, GL_STATIC_DRAW));
			GfxCall(glBindBuffer(GL_ARRAY_BUFFER, s_gl_bone_vbo));
			GfxCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (GLvoid*)vertices, GL_STATIC_DRAW));
			GfxCall(glEnableVertexArrayAttrib(s_gl_bone_vao, 0));
			GfxCall(glVertexAttribPointer(
				0, 3, GL_FLOAT, false, 0, (void const*)(0)
			));
			GfxCall(glEnableVertexArrayAttrib(s_gl_bone_vao, 2));
			GfxCall(glVertexAttribPointer(
				2, 3, GL_FLOAT, false, 0, (void const*)(0)
			));
			GfxCall(glBindVertexArray(0));
		}


	}

	void create_line_mesh()
	{
		glCreateVertexArrays(1, &s_gl_line_vao);
		glCreateBuffers(1, &s_gl_line_vbo);
		glCreateBuffers(1, &s_gl_line_ibo);

		glBindVertexArray(s_gl_line_vao);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_line_ibo);
		unsigned int line_indices[MAX_LINE_POINTS];
		for (unsigned int i = 0; i < MAX_LINE_POINTS; ++i)
			line_indices[i] = i;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_LINE_POINTS * sizeof(unsigned int), line_indices, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, s_gl_line_vbo);
		glBufferData(GL_ARRAY_BUFFER, MAX_LINE_POINTS * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexArrayAttrib(s_gl_line_vao, 0);
		glVertexAttribPointer(
			0, 3, GL_FLOAT, false, 0, (void const*)0
		);

		glBindVertexArray(0);

		glObjectLabel(GL_VERTEX_ARRAY, s_gl_line_vao, -1, "Line VAO");
		glObjectLabel(GL_BUFFER, s_gl_line_vbo, -1, "Line VBO");
		glObjectLabel(GL_BUFFER, s_gl_line_ibo, -1, "Line IBO");
	}

	void set_line_mesh(glm::vec3 const* _points, unsigned int _point_count)
	{
		glBindBuffer(GL_ARRAY_BUFFER, s_gl_line_vbo);
		glBufferSubData(
			GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * _point_count, _points
		);
	}

	ubo_camera_data::ubo_camera_data(
		Engine::Math::transform3D _cam_transform, 
		Engine::Graphics::camera_data _cam_data
	) : 
		m_near(_cam_data.m_near),
		m_far(_cam_data.m_far),
		m_view_dir(_cam_transform.quaternion * glm::vec3(0.0f,0.0f,-1.0f)),
		m_v(_cam_transform.GetInvMatrix()),
		m_p(_cam_data.get_perspective_matrix()),
		m_inv_vp(glm::inverse(_cam_data.get_perspective_matrix() * _cam_transform.GetInvMatrix()))
	{}

}