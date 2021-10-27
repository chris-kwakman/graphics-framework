#ifndef SANDBOX_RENDER_COMMON_H
#define SANDBOX_RENDER_COMMON_H

#include <Engine/Graphics/manager.h>
#include <Engine/Math/Transform3D.h>

namespace Sandbox
{

	using texture_handle = Engine::Graphics::texture_handle;
	using mesh_handle = Engine::Graphics::mesh_handle;
	using shader_program_handle = Engine::Graphics::shader_program_handle;
	using framebuffer_handle = Engine::Graphics::framebuffer_handle;

	// TODO: Destroy these GL objects properly (at some point in the distant future, probably)
	extern unsigned int s_gl_tri_ibo;
	extern unsigned int s_gl_tri_vao;
	extern unsigned int s_gl_tri_vbo;

	extern unsigned int s_gl_bone_vao, s_gl_bone_vbo, s_gl_bone_ibo;
	extern unsigned int s_gl_joint_vao, s_gl_joint_vbo, s_gl_joint_ibo;

	extern unsigned int joint_index_count, bone_index_count;

	// Activate texture on explicit program.
	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index);

	void create_skeleton_bone_model();
}
#endif // !SANDBOX_RENDER_COMMON_H
