#ifndef SANDBOX_RENDER_COMMON_H
#define SANDBOX_RENDER_COMMON_H

#include <Engine/Graphics/manager.h>
#include <Engine/Math/Transform3D.h>

namespace Sandbox
{

	using namespace Engine::Graphics;
	using RM = ResourceManager;
	using texture_handle = RM::texture_handle;
	using mesh_handle = RM::mesh_handle;
	using shader_program_handle = RM::shader_program_handle;
	using framebuffer_handle = RM::framebuffer_handle;

	extern unsigned int s_gl_tri_ibo;
	extern unsigned int s_gl_tri_vao;
	extern unsigned int s_gl_tri_vbo;

	// Activate texture on explicit program.
	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index);

}
#endif // !SANDBOX_RENDER_COMMON_H
