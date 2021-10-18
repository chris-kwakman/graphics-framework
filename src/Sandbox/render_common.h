
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

	// Activate texture on explicit program.
	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index);

}