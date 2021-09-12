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

	using framebuffer_handle = Engine::Graphics::ResourceManager::framebuffer_handle;
	using texture_handle = Engine::Graphics::ResourceManager::texture_handle;

	static framebuffer_handle s_framebuffer;

	static GLuint s_gl_tri_vao, s_gl_tri_ibo, s_gl_tri_vbo;

	static texture_handle s_fb_texture_base_color, s_fb_texture_normal, s_fb_texture_metallic_roughness;

	bool Initialize()
	{
		Engine::Graphics::ResourceManager & system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		system_resource_manager.Reset();

		using shader_handle = Engine::Graphics::ResourceManager::shader_handle;
		using shader_program_handle = Engine::Graphics::ResourceManager::shader_program_handle;

		std::vector<std::filesystem::path> shader_paths = {
			//"data/shaders/default.vert",
			//"data/shaders/default.frag"
			"data/shaders/default.vert",
			"data/shaders/deferred.frag",
			"data/shaders/pbr.vert",
			"data/shaders/pbr.frag",
		};
		std::vector<Engine::Graphics::ResourceManager::shader_handle> output_shader_handles;
		output_shader_handles = system_resource_manager.LoadShaders(shader_paths);

		std::vector<shader_handle> const draw_gbuffer_shaders = { output_shader_handles[0], output_shader_handles[1] };
		std::vector<shader_handle> const draw_pbr_shaders = { output_shader_handles[2], output_shader_handles[3] };

		system_resource_manager.LoadShaderProgram("draw_gbuffer", draw_gbuffer_shaders);
		system_resource_manager.LoadShaderProgram("draw_pbr", draw_pbr_shaders);

		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		glm::uvec2 const surface_size(surface->w, surface->h);

		// Create framebuffer & textures.
		s_framebuffer = system_resource_manager.CreateFramebuffer();

		texture_handle const s_fb_texture_depth = system_resource_manager.CreateTexture();
		s_fb_texture_base_color = system_resource_manager.CreateTexture();
		s_fb_texture_normal = system_resource_manager.CreateTexture();
		s_fb_texture_metallic_roughness = system_resource_manager.CreateTexture();

		//TODO: Incomplete texture when using GL_DEPTH_COMPONENT and GL_R32F
		//system_resource_manager.SpecifyTexture2D(fb_texture_depth, GL_R32F, surface_size);
		system_resource_manager.SpecifyAndUploadTexture2D(s_fb_texture_depth, GL_DEPTH_COMPONENT, surface_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		system_resource_manager.SpecifyTexture2D(s_fb_texture_base_color, GL_RGB, surface_size);
		system_resource_manager.SpecifyTexture2D(s_fb_texture_metallic_roughness, GL_RG, surface_size);
		system_resource_manager.SpecifyTexture2D(s_fb_texture_normal, GL_RGB16F, surface_size);

		// Attach textures to framebuffer.
		system_resource_manager.BindFramebuffer(s_framebuffer);
		system_resource_manager.AttachTextureToFramebuffer(s_framebuffer, GL_DEPTH_ATTACHMENT, s_fb_texture_depth);
		system_resource_manager.AttachTextureToFramebuffer(s_framebuffer, GL_COLOR_ATTACHMENT0, s_fb_texture_base_color);
		system_resource_manager.AttachTextureToFramebuffer(s_framebuffer, GL_COLOR_ATTACHMENT1, s_fb_texture_metallic_roughness);
		system_resource_manager.AttachTextureToFramebuffer(s_framebuffer, GL_COLOR_ATTACHMENT2, s_fb_texture_normal);

		// Load glTF model
		bool success = system_resource_manager.LoadModel(
			"data/gltf/sponza/Sponza.gltf"
			//"data/gltf/japanese-eastern-toad-b-j-formosus/source/Q11442-1all.gltf"
		);
		if (success)
			printf("Asset loaded successfully.\n");
		else
			printf("Failed to load asset.\n");
		
		// Create primitive that covers screen for rendering g-buffer to default framebuffer
		glm::vec2 tri_vert_pos_uv[6] = { 
			{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}, // Define triangle in [-1,1] space
			{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}	
		};
		unsigned char tri_indices[3] = { 0,1,2 };

		//TODO: Implement some way for graphics resource manager to submit meshes, collections of primitives and IBO's easily.

		glGenVertexArrays(1, &s_gl_tri_vao);
		glGenBuffers(1, &s_gl_tri_ibo);
		glGenBuffers(1, &s_gl_tri_vbo);

		glBindVertexArray(s_gl_tri_vao);

		// Bind VBO
		glBindBuffer(GL_ARRAY_BUFFER, s_gl_tri_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(tri_vert_pos_uv), tri_vert_pos_uv, GL_STATIC_DRAW);
		// Set POS and UV attributes
		glEnableVertexArrayAttrib(s_gl_tri_vao, 0);
		glEnableVertexArrayAttrib(s_gl_tri_vao, 1);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, (void*)(sizeof(tri_vert_pos_uv)/2));

		// Bind IBO to VAO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(tri_indices), tri_indices, GL_STATIC_DRAW);

		glBindVertexArray(0);

		GfxCall(glDepthRange(-1.0f, 1.0f));

		return true;
	}

	unsigned int frame_counter = 0;

	float const theta = (float)M_PI * 0.00125f;
	glm::quat camera_rotate(cosf(theta * 0.5f), 0.0f, sinf(theta * 0.5f), 0.0f);

	Engine::Math::transform3D camera_transform;

	void Update()
	{
		frame_counter++;
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		GfxCall(glViewport(0, 0, Singleton<Engine::sdl_manager>().m_surface->w, Singleton<Engine::sdl_manager>().m_surface->h));

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

		shader_program_handle const program_draw_gbuffer = system_resource_manager.FindShaderProgram("draw_gbuffer");
		shader_program_handle const program_draw_pbr = system_resource_manager.FindShaderProgram("draw_pbr");
		assert(program_draw_gbuffer != 0);

		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		float const ar = (float)surface->w / (float)surface->h;

		Engine::Math::transform3D mv = camera_transform.GetInverse() * translation_transform;
		glm::mat4x4 const mvp = camera.create_view_to_perspective_matrix(MATH_PI * 0.4f, ar, 0.5f, 1500.0f) * mv.GetMatrix();

		system_resource_manager.RefreshShaders();

		system_resource_manager.UseProgram(program_draw_gbuffer);
		system_resource_manager.SetBoundProgramUniform(5, mvp);
		system_resource_manager.SetBoundProgramUniform(6, glm::transpose(mv.GetInvMatrix()));

		auto const& mesh_primitives = (system_resource_manager.m_mesh_primitives_map.begin())->second;

		system_resource_manager.BindFramebuffer(s_framebuffer);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLenum attachment_points[] = { GL_NONE, GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		system_resource_manager.DrawFramebuffers(s_framebuffer, sizeof(attachment_points) / sizeof(GLenum), attachment_points);


		auto activate_texture = [&](ResourceManager::texture_handle _texture, unsigned int _shader_sampler_uniform_location, unsigned int _active_texture_index)
		{
			// If no texture handle exists, ignore
			if (_texture == 0)
				return;
			auto texture_info = system_resource_manager.m_texture_info_map.at(_texture);
			GfxCall(glActiveTexture(GL_TEXTURE0 + _active_texture_index));
			GfxCall(glBindTexture(texture_info.m_target, texture_info.m_gl_source_id));

			system_resource_manager.SetBoundProgramUniform(_shader_sampler_uniform_location, (int)_active_texture_index);
		};

		for (unsigned int i = 0; i < mesh_primitives.size(); ++i)
		{
			ResourceManager::mesh_primitive_data primitive = mesh_primitives[i];
			ResourceManager::buffer_info const index_buffer_info = system_resource_manager.m_buffer_info_map.at(primitive.m_index_buffer_handle);
			ResourceManager::index_buffer_info const ibo_info = system_resource_manager.m_index_buffer_info_map.at(primitive.m_index_buffer_handle);


			// Set texture slots
			ResourceManager::material_data material = system_resource_manager.m_material_data_map.at(primitive.m_material_handle);

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

		system_resource_manager.UnbindFramebuffer();
		glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		system_resource_manager.UseProgram(program_draw_pbr);
		using texture_info = Engine::Graphics::ResourceManager::texture_info;

		activate_texture(s_fb_texture_base_color, 0, 0);
		activate_texture(s_fb_texture_metallic_roughness, 1, 1);
		activate_texture(s_fb_texture_normal, 2, 2);

		GfxCall(glBindVertexArray(s_gl_tri_vao));
		GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo));
		GfxCall(glDrawElements(
			GL_TRIANGLES,
			3,
			GL_UNSIGNED_BYTE,
			nullptr
		));
		glBindVertexArray(0);
	}

	void Shutdown()
	{
		Singleton<Engine::Graphics::ResourceManager>().DeleteAllGraphicsResources();
	}
}