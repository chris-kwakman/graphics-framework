#include "sandbox.h"

#include <Engine/Math/Transform.h>
#include <Engine/Graphics/sdl_window.h>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera.h>
#include <Engine/Utils/singleton.h>

#include <glm/gtc/matrix_transform.hpp>

#include <STB/stb_image_write.h>

#include <math.h>

# define MATH_PI           3.14159265358979323846f

namespace Sandbox
{

	// Saves the front framebuffer to an image with the specified filename
	// Call it after swapping buffers to make sure you save the last frame rendered
	void SaveScreenShot(const char* _filename)
	{
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		int x = viewport[0];
		int y = viewport[1];
		int width = viewport[2];
		int height = viewport[3];

		std::vector<unsigned char> imageData(width * height * 4, 0);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
		stbi_flip_vertically_on_write(true);
		stbi_write_png(_filename, width, height, 4, imageData.data(), 0);
	}

	bool Initialize()
	{
		Engine::Graphics::ResourceManager & system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		bool success = system_resource_manager.LoadModel(
			"data/gltf/sponza/Sponza.gltf"
			//"data/gltf/japanese-eastern-toad-b-j-formosus/source/Q11442-1all.gltf"
		);
		if (success)
			printf("Asset loaded successfully.\n");
		else
			printf("Failed to load asset.\n");

		using shader_program_handle = Engine::Graphics::ResourceManager::shader_program_handle;

		std::vector<std::filesystem::path> shader_paths = {
			"data/shaders/default.vert",
			"data/shaders/default.frag"
		};
		std::vector<Engine::Graphics::ResourceManager::shader_handle> output_shader_handles;
		system_resource_manager.LoadShaders(shader_paths);
		shader_program_handle program = system_resource_manager.LoadShaderProgram(shader_paths);

		GfxCall(glDepthRange(-1.0f, 1.0f));
		GfxCall(glViewport(0, 0, Singleton<Engine::sdl_manager>().m_surface->w, Singleton<Engine::sdl_manager>().m_surface->h));

		return true;
	}

	unsigned int frame_counter = 0;

	float const theta = (float)M_PI * 0.00125f;
	glm::quat camera_rotate(cosf(theta * 0.5f), 0.0f, sinf(theta * 0.5f), 0.0f);

	Engine::Math::transform3D camera_transform;

	void Update()
	{
		frame_counter++;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		using index_buffer_handle = Engine::Graphics::ResourceManager::buffer_handle;
		using namespace Engine::Graphics;

		Engine::Math::transform3D translation_transform;

		translation_transform.position = glm::vec3(0.0f, 0.0f, -10.0f);
		camera_transform.quaternion = camera_rotate * camera_transform.quaternion;
		camera_transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
		
		//camera_transform.position.y = 200.0f * std::sinf((float)frame_counter / (10.0f * MATH_PI));

		Engine::Graphics::camera camera;
		camera.m_right = glm::vec3(1.0f, 0.0f, 0.0f);
		camera.m_up = glm::vec3(0.0f, 1.0f, 0.0f);

		//camera_transform.quaternion = camera.get_lookat_quat();

		Engine::Graphics::ResourceManager& system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		using shader_program_handle = Engine::Graphics::ResourceManager::shader_program_handle;

		shader_program_handle program = system_resource_manager.m_shader_program_info_map.begin()->first;

		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		float const ar = (float)surface->w / (float)surface->h;

		Engine::Math::transform3D mv = camera_transform.GetInverse() * translation_transform;
		glm::mat4x4 const mvp = camera.create_view_to_perspective_matrix(MATH_PI * 0.4f, ar, 0.5f, 1500.0f) * mv.GetMatrix();

		system_resource_manager.RefreshShaders();

		system_resource_manager.UseProgram(program);
		system_resource_manager.SetBoundProgramUniform(5, mvp);
		system_resource_manager.SetBoundProgramUniform(6, glm::transpose(mv.GetInvMatrix()));

		auto const& mesh_primitives = (system_resource_manager.m_mesh_primitives_map.begin())->second;

		for (unsigned int i = 0; i < mesh_primitives.size(); ++i)
		{
			ResourceManager::mesh_primitive_data primitive = mesh_primitives[i];
			ResourceManager::buffer_info const index_buffer_info = system_resource_manager.m_buffer_info_map.at(primitive.m_index_buffer_handle);
			ResourceManager::index_buffer_info const ibo_info = system_resource_manager.m_index_buffer_info_map.at(primitive.m_index_buffer_handle);


			// Set texture slots
			ResourceManager::material_data material = system_resource_manager.m_material_data_map.at(primitive.m_material_handle);

			auto activate_texture = [&](ResourceManager::texture_handle _texture, unsigned int _shader_sampler_uniform_location, unsigned int _active_texture_index)
			{
				// If no texture handle exists, ignore
				if (_texture == 0)
					return;
				auto texture_source = system_resource_manager.m_texture_source_info_map.at(
					system_resource_manager.m_texture_info_map.at(_texture).m_source_handle
				);
				glActiveTexture(GL_TEXTURE0 + _active_texture_index);
				glBindTexture(texture_source.m_target, texture_source.m_gl_source_id);

				system_resource_manager.SetBoundProgramUniform(_shader_sampler_uniform_location, (int)_active_texture_index);
			};

			activate_texture(material.m_pbr_metallic_roughness.m_texture_base_color, 0, 0);
			activate_texture(material.m_pbr_metallic_roughness.m_texture_metallic_roughness, 1, 1);
			activate_texture(material.m_texture_normal, 2, 2);
			activate_texture(material.m_texture_occlusion, 3, 3 );
			activate_texture(material.m_texture_emissive, 4, 4);

			using alpha_mode = ResourceManager::material_data::alpha_mode;
			system_resource_manager.SetBoundProgramUniform(
				10,
				(float)(material.m_alpha_mode == alpha_mode::eMASK ? material.m_alpha_cutoff : 0.0)
			);
			if (material.m_alpha_mode == alpha_mode::eBLEND)
				glEnable(GL_BLEND);
			else
				glDisable(GL_BLEND);

			if (material.m_double_sided)
				glCullFace(GL_FALSE);
			else
				glCullFace(GL_TRUE);

			GfxCall(glBindVertexArray(primitive.m_vao_gl_id));
			GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_info.m_gl_id));
			GfxCall(glDrawElements(
				primitive.m_render_mode,
				(GLsizei)ibo_info.m_index_count,
				ibo_info.m_type,
				nullptr
			));
			glBindVertexArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			
		}
	}

	void Shutdown()
	{
	}
}