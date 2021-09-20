#include "light.h"
#include <Engine/Utils/singleton.h>
#include <Engine/Math/Transform.h>
#include <Engine/Graphics/sdl_window.h>

namespace Engine {
namespace Graphics {

	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;

	void RenderLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera _camera,
		Engine::Math::transform3D _camera_transform,
		light const* _lights, unsigned int _light_count
	)
	{
		using namespace Engine::Graphics;

		auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		// OpenGL setup
		glCullFace(GL_FRONT);
		glDepthFunc(GL_GREATER);
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_ONE, GL_ONE);

		auto const & primitives = resource_manager.GetMeshPrimitives(_light_mesh);
		ResourceManager::mesh_primitive_data light_primitive = primitives.front();
		glBindVertexArray(light_primitive.m_vao_gl_id);
		ResourceManager::index_buffer_info ibo_info = resource_manager.GetIndexBufferInfo(light_primitive.m_index_buffer_handle);
		
		auto surface = Singleton<Engine::sdl_manager>().m_surface;
		glm::uvec2 const viewport_size(surface->w, surface->h);
		glm::vec2 const viewport_sizef(viewport_size);

		resource_manager.SetBoundProgramUniform(10, viewport_size);
		resource_manager.SetBoundProgramUniform(11, _camera.m_near);
		resource_manager.SetBoundProgramUniform(12, _camera.m_far);

		glm::mat4x4 const view_matrix = _camera_transform.GetInvMatrix();
		glm::mat4x4 const perspective_matrix = _camera.create_view_to_perspective_matrix();
		glm::mat4x4 const view_perspective_matrix = perspective_matrix * view_matrix;

		// Upload inverse perspective matrix for unprojecting fragment depth in shader code.
		resource_manager.SetBoundProgramUniform(7, glm::inverse(perspective_matrix));
		// Upload world to view matrix that will be used to bring normal space vectors to view space.
		resource_manager.SetBoundProgramUniform(8, view_matrix);

		glm::mat4x4 const translation(1);
		glm::mat4x4 const scale(1);
		for (unsigned int i = 0; i < _light_count; ++i)
		{
			light const current_light = _lights[i];

			glm::mat4x4 const m = glm::translate(translation, current_light.m_pos) *
				glm::scale(scale, 2.0f * glm::vec3(current_light.m_radius, current_light.m_radius, current_light.m_radius));

			glm::vec3 const light_view_pos = glm::vec3(view_matrix * glm::vec4(current_light.m_pos, 1));

			//printf("Light view pos: (%.1f, %.1f, %.1f)\n", light_view_pos.x, light_view_pos.y, light_view_pos.z);

			resource_manager.SetBoundProgramUniform(5, view_perspective_matrix * m);
			glm::mat4 const view_model_matrix = view_matrix * m;
			//resource_manager.SetBoundProgramUniform(6, glm::transpose(glm::inverse(view_model_matrix)));
			//resource_manager.SetBoundProgramUniform(9, view_model_matrix);
			resource_manager.SetBoundProgramUniform(13, light_view_pos);
			resource_manager.SetBoundProgramUniform(14, current_light.m_radius);
			resource_manager.SetBoundProgramUniform(15, current_light.m_color);

			glDrawElements(
				light_primitive.m_render_mode,
				(GLsizei)ibo_info.m_index_count,
				ibo_info.m_type,
				(void*)0
			);
		}

		glBindVertexArray(0);
	}

}
}
