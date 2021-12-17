#ifndef SANDBOX_RENDER_COMMON_H
#define SANDBOX_RENDER_COMMON_H

#include <Engine/Graphics/manager.h>
#include <Engine/Math/Transform3D.h>
#include <Engine/Graphics/camera_data.h>

namespace Sandbox
{
	using texture_handle = Engine::Graphics::texture_handle;
	using mesh_handle = Engine::Graphics::mesh_handle;
	using shader_program_handle = Engine::Graphics::shader_program_handle;
	using framebuffer_handle = Engine::Graphics::framebuffer_handle;
	using mesh_primitive_data = Engine::Graphics::ResourceManager::mesh_primitive_data;

	// Graphics Objects

# define MATH_PI           3.14159265358979323846f

	extern texture_handle
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

	extern texture_handle s_texture_white;

	extern framebuffer_handle
		s_framebuffer_gbuffer,
		s_framebuffer_gbuffer_decal,
		s_framebuffer_lighting,
		s_framebuffer_bloom[2],
		s_framebuffer_shadow,
		s_framebuffer_ao,
		s_framebuffer_ao_pingpong
		;

	// Lighting data
	extern glm::vec4		s_clear_color;
	extern glm::vec3		s_ambient_color;
	extern float			s_exposure;
	extern float			s_gamma_correction_factor;
	extern float			s_shininess_mult_factor;
	// Blur data
	extern bool				s_bloom_enabled;
	extern glm::uvec2		s_bloom_texture_size;
	extern glm::vec3		s_bloom_treshhold_color;
	extern unsigned int		s_bloom_blur_count;

	extern double			s_time;


	// Ambient occlusion data
	struct GfxAmbientOcclusion
	{
		// AO Rendering Settings
		bool disable = true;

		// AO Configuration
		float radius_scale = 7.0f;
		float angle_bias = 3.1415f / 30.0f;
		float attenuation_scale = 1.0f;
		float ao_scale = 1.0f;
		int sample_directions = 6;
		int sample_steps = 4;

		float sigma = 4.0f;
		int blur_passes = 6;

		enum RenderMode {eAO_Applied = 0, eAO_Monochrome = 1};
		RenderMode render_mode{ eAO_Applied };
	};
	extern GfxAmbientOcclusion s_ambient_occlusion;

	// Editor Data
	extern bool				s_render_infinite_grid;

	struct ubo_camera_data
	{
		static GLuint const BINDING_POINT = 64;

		ubo_camera_data(Engine::Math::transform3D _cam_transform, Engine::Graphics::camera_data _cam_data);
		glm::mat4 m_inv_vp, m_v, m_p;
		glm::vec2 m_viewport_size;
		float m_near;
		float m_far;
		glm::vec3 m_view_dir;
		float _padding;
	};
	extern GLuint s_ubo_camera;

	// Miscellaneous Graphics Stuff

	// TODO: Destroy these GL objects properly (at some point in the distant future, probably)
	extern unsigned int s_gl_tri_ibo;
	extern unsigned int s_gl_tri_vao;
	extern unsigned int s_gl_tri_vbo;

	extern unsigned int s_gl_line_vao;
	extern unsigned int s_gl_line_vbo;
	extern unsigned int s_gl_line_ibo;

	extern unsigned int s_gl_bone_vao, s_gl_bone_vbo, s_gl_bone_ibo;
	extern unsigned int s_gl_joint_vao, s_gl_joint_vbo, s_gl_joint_ibo;

	extern unsigned int joint_index_count, bone_index_count;

	void setup_render_common();
	void shutdown_render_common();

	void update_camera_ubo(ubo_camera_data _camera_data);

	// Activate texture on explicit program.
	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index);
	void render_primitive(mesh_primitive_data const& _primitive);

	void create_skeleton_bone_model();

	unsigned int const MAX_LINE_POINTS = 1024;

	void create_line_mesh();
	void set_line_mesh(glm::vec3 const* _points, unsigned int _point_count);

}
#endif // !SANDBOX_RENDER_COMMON_H
