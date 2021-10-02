#include "lighting_pass.h"
#include <Engine/Utils/singleton.h>
#include <Engine/Math/Transform3D.h>
#include <Engine/Graphics/sdl_window.h>
#include <Engine/Components/Light.h>

namespace Sandbox {

	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;

	void RenderLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
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

		auto const& primitives = resource_manager.GetMeshPrimitives(_light_mesh);
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
		glm::mat4x4 const perspective_matrix = _camera.get_perspective_matrix();
		glm::mat4x4 const view_perspective_matrix = perspective_matrix * view_matrix;

		// Upload inverse perspective matrix for unprojecting fragment depth in shader code.
		resource_manager.SetBoundProgramUniform(7, glm::inverse(perspective_matrix));
		// Upload world to view matrix that will be used to bring normal space vectors to view space.
		resource_manager.SetBoundProgramUniform(8, view_matrix);

		glm::mat4x4 const translation(1);
		glm::mat4x4 const scale(1);

		auto collection = Singleton<Component::PointLightManager>().GetPointLightCollection();

		for (unsigned int i = 0; i < collection.m_light_count; ++i)
		{

			glm::mat4x4 const m = glm::translate(translation, collection.m_light_pos_arr[i]) *
				glm::scale(scale, 2.0f * glm::vec3(collection.m_light_radius_arr[i]));

			glm::vec3 const light_view_pos = glm::vec3(view_matrix * glm::vec4(collection.m_light_pos_arr[i], 1));

			resource_manager.SetBoundProgramUniform(5, view_perspective_matrix * m);
			glm::mat4 const view_model_matrix = view_matrix * m;
			//resource_manager.SetBoundProgramUniform(6, glm::transpose(glm::inverse(view_model_matrix)));
			//resource_manager.SetBoundProgramUniform(9, view_model_matrix);
			resource_manager.SetBoundProgramUniform(13, light_view_pos);
			resource_manager.SetBoundProgramUniform(14, collection.m_light_radius_arr[i]);
			resource_manager.SetBoundProgramUniform(15, collection.m_light_color_arr[i]);

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
