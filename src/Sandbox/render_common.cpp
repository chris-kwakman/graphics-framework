
#include "render_common.h"
#include <Engine/Utils/singleton.h>

namespace Sandbox
{

	void activate_texture(texture_handle _texture, unsigned int _program_uniform_index, unsigned int _texture_index)
	{
		// If no texture handle exists, ignore
		if (_texture == 0)
			return;

		auto & res_mgr = Singleton<RM>();
		auto const texture_info = res_mgr.GetTextureInfo(_texture);
		GfxCall(glActiveTexture(GL_TEXTURE0 + _texture_index));
		GfxCall(glBindTexture(texture_info.m_target, texture_info.m_gl_source_id));

		res_mgr.SetBoundProgramUniform(_program_uniform_index, (int)_texture_index);
	}

}