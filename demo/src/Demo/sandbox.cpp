#include "sandbox.h"

#include "render_common.h"
#include "graphics_pipeline.h"

#include <Engine/Math/Transform3D.h>
#include <Engine/Graphics/sdl_window.h>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera_data.h>
#include <Engine/Utils/singleton.h>
#include <Engine/Managers/input.h>
#include <Engine/Editor/editor.h>
#include <Engine/Serialisation/serialise_gltf.h>
#include <Engine/Utils/logging.h>

#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Rigidbody.h>
#include <Engine/Editor/EditorCameraController.h>

#include <Demo/Components/PlayerController.h>

#include <stb_image_write.h>

#include "LoadScene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GLM/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui_stdlib.h>

#include <string>
#include <math.h>
#include <iostream>
#include <iomanip>

#include <fstream>

#include <glm/gtx/string_cast.hpp>
#include <Engine/Graphics/misc/create_convex_hull_mesh.h>
#include <Engine/Physics/intersection.h>
#include <Engine/Physics/Collider.h>

Engine::Graphics::texture_handle		s_display_gbuffer_texture = 0;

static Engine::Math::transform3D s_camera_default_transform;

namespace Sandbox
{
	static std::string s_scene_loaded;
	static bool s_bool_save_screenshot = false;
	static std::string s_saved_screenshot_name;

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
		glReadBuffer(GL_BACK);
		glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
		stbi_flip_vertically_on_write(true);
		stbi_write_png(_filename, width, height, 4, imageData.data(), 0);
	}

	static std::vector<texture_handle> g_gbuffer_textures;

	using Entity = Engine::ECS::Entity;

	void setup_shaders()
	{
		Engine::Graphics::ResourceManager& system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		using shader_handle = Engine::Graphics::ResourceManager::shader_handle;
		using shader_program_handle = Engine::Graphics::shader_program_handle;

		std::vector<fs::path> shader_paths = {
			"data/shaders/infinite_grid.vert",
			"data/shaders/infinite_grid.frag",
			"data/shaders/default.vert",
			"data/shaders/skinned.vert",
			"data/shaders/deferred.frag",
			"data/shaders/deferred_decal.frag",
			"data/shaders/display_framebuffer.vert",
			"data/shaders/display_framebuffer_plain.frag",
			"data/shaders/display_framebuffer_global_light.frag",
			"data/shaders/display_framebuffer_ambient_occlusion.frag",
			"data/shaders/display_framebuffer_ao_monochrome.frag",
			"data/shaders/display_framebuffer_clearcolor.frag",
			"data/shaders/blur_ambient_occlusion.frag",
			"data/shaders/phong.frag",
			"data/shaders/bloom.frag",
			"data/shaders/postprocessing.frag",
			"data/shaders/dirlight_shadowmap.vert",
			"data/shaders/dirlight_shadowmap.frag",
			"data/shaders/write_csm.vert",
			"data/shaders/write_csm.frag",
			"data/shaders/primitive.frag",
			"data/shaders/draw_collider_debug.vert",
			"data/shaders/draw_collider_debug.frag"
		};
		std::vector<Engine::Graphics::ResourceManager::shader_handle> output_shader_handles;
		output_shader_handles = system_resource_manager.LoadShaders(shader_paths);

		using program_shader_path_list = std::vector<fs::path>;

		program_shader_path_list const draw_infinite_grid_shaders = { "data/shaders/infinite_grid.vert", "data/shaders/infinite_grid.frag" };
		program_shader_path_list const draw_gbuffer_skinned_shaders = { "data/shaders/skinned.vert", "data/shaders/deferred.frag" };
		program_shader_path_list const draw_gbuffer_shaders = { "data/shaders/default.vert", "data/shaders/deferred.frag" };
		program_shader_path_list const draw_gbuffer_decals = { "data/shaders/default.vert", "data/shaders/deferred_decal.frag" };
		program_shader_path_list const draw_gbuffer_primitive = { "data/shaders/default.vert", "data/shaders/primitive.frag" };
		program_shader_path_list const draw_framebuffer_plain_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_plain.frag" };
		program_shader_path_list const draw_framebuffer_global_light = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_global_light.frag" };
		program_shader_path_list const draw_framebuffer_ambient_occlusion = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_ambient_occlusion.frag" };
		program_shader_path_list const draw_framebuffer_ao_monochrome = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_ao_monochrome.frag" };
		program_shader_path_list const draw_framebuffer_clearcolor = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_clearcolor.frag" };
		program_shader_path_list const blur_ambient_occlusion_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/blur_ambient_occlusion.frag" };
		program_shader_path_list const draw_phong_shaders = { "data/shaders/default.vert", "data/shaders/phong.frag" };
		program_shader_path_list const process_bloom_blur_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/bloom.frag" };
		program_shader_path_list const draw_postprocessing_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/postprocessing.frag" };
		program_shader_path_list const directional_light_shaders = { "data/shaders/dirlight_shadowmap.vert", "data/shaders/dirlight_shadowmap.frag" };
		program_shader_path_list const write_csm_shaders = { "data/shaders/write_csm.vert", "data/shaders/write_csm.frag" };
		program_shader_path_list const draw_collider_debug_shaders = { "data/shaders/draw_collider_debug.vert", "data/shaders/draw_collider_debug.frag" };


		system_resource_manager.LoadShaderProgram("draw_infinite_grid", draw_infinite_grid_shaders);
		system_resource_manager.LoadShaderProgram("draw_gbuffer", draw_gbuffer_shaders);
		system_resource_manager.LoadShaderProgram("draw_gbuffer_skinned", draw_gbuffer_skinned_shaders);
		system_resource_manager.LoadShaderProgram("draw_gbuffer_decals", draw_gbuffer_decals);
		system_resource_manager.LoadShaderProgram("draw_gbuffer_primitive", draw_gbuffer_primitive);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_plain", draw_framebuffer_plain_shaders);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_global_light", draw_framebuffer_global_light);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_ambient_occlusion", draw_framebuffer_ambient_occlusion);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_ao_monochrome", draw_framebuffer_ao_monochrome);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_clearcolor", draw_framebuffer_clearcolor);
		system_resource_manager.LoadShaderProgram("blur_ambient_occlusion", blur_ambient_occlusion_shaders);
		system_resource_manager.LoadShaderProgram("draw_phong", draw_phong_shaders);
		system_resource_manager.LoadShaderProgram("process_bloom_blur", process_bloom_blur_shaders);
		system_resource_manager.LoadShaderProgram("postprocessing", draw_postprocessing_shaders);
		system_resource_manager.LoadShaderProgram("dirlight_shadowmap", directional_light_shaders);
		system_resource_manager.LoadShaderProgram("write_csm", write_csm_shaders);
		system_resource_manager.LoadShaderProgram("draw_collider_debug", draw_collider_debug_shaders);
	}

	void setup_framebuffer()
	{
		Engine::Graphics::ResourceManager& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		glm::uvec2 const surface_size(surface->w, surface->h);

		// Create framebuffer & textures.
		s_framebuffer_gbuffer = resource_manager.CreateFramebuffer();
		s_framebuffer_gbuffer_decal = resource_manager.CreateFramebuffer();
		s_framebuffer_lighting = resource_manager.CreateFramebuffer();
		s_framebuffer_shadow = resource_manager.CreateFramebuffer();
		for (unsigned int i = 0; i < 2; ++i)
		{
			s_framebuffer_bloom[i] = resource_manager.CreateFramebuffer();
		}
		s_framebuffer_ao = resource_manager.CreateFramebuffer();
		s_framebuffer_ao_pingpong = resource_manager.CreateFramebuffer();

		s_fb_texture_depth = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Depth Texture");
		s_fb_texture_base_color = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Base Color Texture");
		s_fb_texture_normal = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Normal Texture");
		s_fb_texture_metallic_roughness = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Metallic Roughness Texture");
		s_fb_texture_light_color = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Light Color");
		s_fb_texture_luminance = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Luminance");
		s_fb_texture_shadow = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Shadow Map");
		s_fb_texture_ao = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Ambient Occlusion");
		for (unsigned int i = 0; i < 2; ++i)
			s_fb_texture_bloom_pingpong[i] = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB Bloom Pingpong");

		s_fb_texture_ao_pingpong = resource_manager.CreateTexture(GL_TEXTURE_2D, "FB AO Pingpong");
		s_texture_white = resource_manager.CreateTexture(GL_TEXTURE_2D, "White Texture");

		s_display_gbuffer_texture = s_fb_texture_ao;

		//resource_manager.SpecifyAndUploadTexture2D(s_fb_texture_depth, GL_DEPTH_COMPONENT, surface_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		using tex_params = Engine::Graphics::ResourceManager::texture_parameters;
		tex_params default_fb_texture_params;
		default_fb_texture_params.m_wrap_s = GL_CLAMP_TO_EDGE;
		default_fb_texture_params.m_wrap_t = GL_CLAMP_TO_EDGE;
		default_fb_texture_params.m_min_filter = GL_LINEAR;
		default_fb_texture_params.m_mag_filter = GL_LINEAR;
		resource_manager.AllocateTextureStorage2D(s_fb_texture_depth, GL_DEPTH_COMPONENT32F, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_base_color, GL_RGBA8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_metallic_roughness, GL_RGB8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_normal, GL_RGB8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_light_color, GL_RGB16F, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_shadow, GL_R16F, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_ao, GL_R16F, surface_size / 1u, default_fb_texture_params);
		tex_params luminance_params = default_fb_texture_params;
		luminance_params.m_min_filter = GL_LINEAR_MIPMAP_LINEAR;
		resource_manager.AllocateTextureStorage2D(s_fb_texture_luminance, GL_RGB16F, surface_size, luminance_params,3);
		s_bloom_texture_size = surface_size / glm::uvec2(4);
		for (unsigned int i = 0; i < 2; ++i)
		{
			// TODO: Half size of bloom blur textures
			resource_manager.AllocateTextureStorage2D(s_fb_texture_bloom_pingpong[i], GL_RGB16F, s_bloom_texture_size, default_fb_texture_params);
		}
		// Screensize AO blurring textures
		resource_manager.AllocateTextureStorage2D(s_fb_texture_ao_pingpong, GL_R16F, surface_size, default_fb_texture_params);

		glm::vec3 color_white{ 1.0f,1.0f,1.0f };
		resource_manager.SpecifyAndUploadTexture2D(s_texture_white, GL_RGB8, glm::uvec2(1, 1), 0, GL_RGB, GL_FLOAT, (void*)&color_white);

		// Attach textures to framebuffer.
		resource_manager.BindFramebuffer(s_framebuffer_gbuffer);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer, GL_DEPTH_ATTACHMENT, s_fb_texture_depth);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer, GL_COLOR_ATTACHMENT0, s_fb_texture_base_color);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer, GL_COLOR_ATTACHMENT1, s_fb_texture_metallic_roughness);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer, GL_COLOR_ATTACHMENT2, s_fb_texture_normal);
		GLenum const gbuffer_attachment_points[] = { GL_NONE, GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		resource_manager.DrawFramebuffers(s_framebuffer_gbuffer, sizeof(gbuffer_attachment_points) / sizeof(GLenum), gbuffer_attachment_points);

		resource_manager.BindFramebuffer(s_framebuffer_gbuffer_decal);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer_decal, GL_COLOR_ATTACHMENT0, s_fb_texture_base_color);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer_decal, GL_COLOR_ATTACHMENT1, s_fb_texture_metallic_roughness);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_gbuffer_decal, GL_COLOR_ATTACHMENT2, s_fb_texture_normal);
		GLenum const gbuffer_decal_attachment_points[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		resource_manager.DrawFramebuffers(s_framebuffer_gbuffer_decal, sizeof(gbuffer_decal_attachment_points) / sizeof(GLenum), gbuffer_decal_attachment_points);

		resource_manager.BindFramebuffer(s_framebuffer_lighting);
		//resource_manager.AttachTextureToFramebuffer(s_framebuffer_lighting, GL_DEPTH_ATTACHMENT, s_fb_texture_depth);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_lighting, GL_COLOR_ATTACHMENT0, s_fb_texture_light_color);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_lighting, GL_COLOR_ATTACHMENT1, s_fb_texture_luminance);
		GLenum const lighting_attachment_points[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
		resource_manager.DrawFramebuffers(s_framebuffer_lighting, sizeof(lighting_attachment_points) / sizeof(GLenum), lighting_attachment_points);

		resource_manager.BindFramebuffer(s_framebuffer_shadow);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_shadow, GL_COLOR_ATTACHMENT0, s_fb_texture_shadow);
		GLenum const shadow_attachment_points[] = { GL_COLOR_ATTACHMENT0 };
		resource_manager.DrawFramebuffers(s_framebuffer_shadow, sizeof(shadow_attachment_points) / sizeof(GLenum), shadow_attachment_points);

		for (unsigned int i = 0; i < 2; ++i)
		{
			resource_manager.BindFramebuffer(s_framebuffer_bloom[i]);
			resource_manager.AttachTextureToFramebuffer(s_framebuffer_bloom[i], GL_COLOR_ATTACHMENT0, s_fb_texture_bloom_pingpong[i]);
			GLenum const blur_texture_attachment_point = GL_COLOR_ATTACHMENT0;
			resource_manager.DrawFramebuffers(s_framebuffer_bloom[i], 1, &blur_texture_attachment_point);
		}

		resource_manager.BindFramebuffer(s_framebuffer_ao);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_ao, GL_COLOR_ATTACHMENT0, s_fb_texture_ao);
		GLenum const ao_attachment_points[] = { GL_COLOR_ATTACHMENT0 };
		resource_manager.DrawFramebuffers(s_framebuffer_ao, 1, ao_attachment_points);

		resource_manager.BindFramebuffer(s_framebuffer_ao_pingpong);
		resource_manager.AttachTextureToFramebuffer(s_framebuffer_ao_pingpong, GL_COLOR_ATTACHMENT0, s_fb_texture_ao_pingpong);
		// Manually change DrawFramebuffers when performing AO blurring.
		GLenum const ao_pingpong_attachment_point = GL_COLOR_ATTACHMENT0;
		resource_manager.DrawFramebuffers(s_framebuffer_ao_pingpong, 1, &ao_pingpong_attachment_point);

		g_gbuffer_textures = {
			s_fb_texture_base_color,
			s_fb_texture_depth,
			s_fb_texture_normal,
			s_fb_texture_metallic_roughness,
			s_fb_texture_light_color,
			s_fb_texture_luminance,
			s_fb_texture_shadow,
			s_fb_texture_ao,
			s_fb_texture_ao_pingpong
		};
	}

	void create_framebuffer_triangle()
	{
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
		glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, (void*)(sizeof(tri_vert_pos_uv) / 2));

		// Bind IBO to VAO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(tri_indices), tri_indices, GL_STATIC_DRAW);

		glBindVertexArray(0);
	}

	void InitializeSandboxComponentManagers()
	{
		Singleton<Component::SceneEntityComponentManager>().Initialize();
	}
	void ShutdownSandboxComponentManagers()
	{
		Singleton<Component::SceneEntityComponentManager>().Clear();
	}

	static bool s_scene_reset = false;
	static bool s_camera_reset = false;

	void ResetEditorCamera()
	{
		s_camera_reset = false;

		Entity& editor_camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
		auto& all_cameras = Singleton<Component::CameraManager>().AllCameras();
		if (!all_cameras.empty())
		{
			editor_camera_entity = all_cameras.begin()->first;
		}
		else
		{
			Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&editor_camera_entity, 1);
			editor_camera_entity.SetName("Editor Camera");
			auto transform_comp = Component::Create<Component::Transform>(editor_camera_entity);
			transform_comp.SetLocalPosition(glm::vec3(0.0f, 5.0f, 5.0f));
			auto cam_comp = Component::Create<Component::Camera>(editor_camera_entity);
			cam_comp.SetFarDistance(200.0f);
			auto editor_cam_controller = Component::Create<Component::EditorCameraController>(editor_camera_entity);
			editor_cam_controller.SetAsActiveEditorCamera();
		}
		s_camera_default_transform = editor_camera_entity.GetComponent<Component::Transform>().GetLocalTransform();
	}

	void ResetSceneData()
	{
		s_scene_reset = false;

		ResetEditorCamera();
	}

	Engine::Physics::convex_hull_handle s_cube_convex_hull_handle;

	bool Initialize(int argc, char* argv[])
	{
		setup_render_common();
		create_framebuffer_triangle();
		setup_shaders();
		setup_framebuffer();

		SetupGraphicsPipelineRender();
		ResetEditorCamera();


		if (argc >= 3)
		{
			s_bool_save_screenshot = true;
			s_saved_screenshot_name = std::string(argv[2]);
			// Request quit right away.
			Singleton<Engine::sdl_manager>().m_want_quit = true;
		}

		s_scene_reset = true;

		GfxCall(glDepthRange(-1.0f, 1.0f));

		return true;
	}

	unsigned int frame_counter = 0;

	bool show_demo_window = false;

	const char * get_texture_name(texture_handle _texture)
	{
		if (s_fb_texture_base_color == _texture)
			return "Diffuse";
		else if (s_fb_texture_depth == _texture)
			return "Depth";
		else if (s_fb_texture_normal == _texture)
			return "Normal";
		else if (s_fb_texture_metallic_roughness == _texture)
			return "Metallic Roughness";
		else if (s_fb_texture_light_color == _texture)
			return "Light Color";
		else if (s_fb_texture_luminance == _texture)
			return "Luminance";
		else if (s_fb_texture_shadow == _texture)
			return "Shadow Map";
		else if (s_fb_texture_ao == _texture)
			return "Ambient Occlusion";
		else if (s_fb_texture_ao_pingpong == _texture)
			return "AO Blur";
		else return "Undefined";
	}

	void DummyEditorRender()
	{
		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		auto& system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		if (ImGui::Begin("Graphics"))
		{
			ImGui::Checkbox("Render Infinite Grid", &s_render_infinite_grid);
			ImGui::ColorEdit3("Clear Color", &s_clear_color.r);

			ImGui::SliderFloat("Shininess Multiplier", &s_shininess_mult_factor, 1.0f, 500.0f, "%.1f");
			ImGui::ColorEdit3("Ambient Light", &s_ambient_color.x);

			ImGui::Separator();

			ImGui::SliderFloat("Gamma Correction", &s_gamma_correction_factor, 1.0f, 5.0f, "%.1f");
			ImGui::SliderFloat("Exposure", &s_exposure, 0.0f, 100.0f, "%.1f");
			if (ImGui::Checkbox("Enable Bloom", &s_bloom_enabled))
			{
				if (!s_bloom_enabled)
				{
					for (unsigned int i = 0; i < 2; ++i)
					{
						system_resource_manager.BindFramebuffer(s_framebuffer_bloom[i]);
						glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
						glClear(GL_COLOR_BUFFER_BIT);
					}
					system_resource_manager.UnbindFramebuffer();
				}
			}
			ImGui::BeginDisabled(!s_bloom_enabled);
			int bloom_blur_count = s_bloom_blur_count;
			if (ImGui::SliderInt("###Bloom Blur Count", &bloom_blur_count, 2, 20, "Bloom Blur Count: %d", ImGuiSliderFlags_AlwaysClamp))
				s_bloom_blur_count = std::max(2, bloom_blur_count);
			ImGui::ColorEdit3("Bloom Luminance Treshhold", &s_bloom_treshhold_color.x);
			ImGui::EndDisabled();

			if (false)
			{
				if (ImGui::BeginCombo("Show GBuffer Texture", get_texture_name(s_display_gbuffer_texture)))
				{
					for (auto gbuffer_texture : g_gbuffer_textures)
					{
						if (ImGui::Selectable(get_texture_name(gbuffer_texture)))
							s_display_gbuffer_texture = gbuffer_texture;
					}
					ImGui::EndCombo();
				}
				auto texture_info = system_resource_manager.GetTextureInfo(s_display_gbuffer_texture);
				glm::vec2 image_size(texture_info.m_size.x, texture_info.m_size.y);
				ImVec2 content_region_avail = ImGui::GetContentRegionAvail();
				float image_ar = image_size.y / image_size.x;
				float content_region_ar = content_region_avail.y / content_region_avail.x;

				ImGui::Image(
					reinterpret_cast<ImTextureID>((size_t)texture_info.m_gl_source_id),
					ImVec2(content_region_avail.x, content_region_avail.x * image_ar),
					ImVec2(0, 1), ImVec2(1, 0)
				);
			}
		}
		ImGui::End();
		if (ImGui::Begin("Ambient Occlusion"))
		{
			ImGui::DragFloat("Radius Scale", &s_ambient_occlusion.radius_scale, 0.01f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Angle Bias", &s_ambient_occlusion.angle_bias, 0.0f, glm::pi<float>() / 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Attenuation Scale", &s_ambient_occlusion.attenuation_scale, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("AO Scale", &s_ambient_occlusion.ao_scale, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderInt("Sample Directions", &s_ambient_occlusion.sample_directions, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderInt("Sample Steps", &s_ambient_occlusion.sample_steps, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("Blur Sigma", &s_ambient_occlusion.sigma, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderInt("Blur Passes", &s_ambient_occlusion.blur_passes, 0, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
			using AORenderMode = GfxAmbientOcclusion::RenderMode;
			auto & current_render_mode = s_ambient_occlusion.render_mode;
			const char* display_text = (current_render_mode == AORenderMode::eAO_Applied) ?
				"Show AO texture" :
				"Show AO applied";
			ImGui::Checkbox("Disable AO Rendering", &s_ambient_occlusion.disable);
			if (ImGui::Button(display_text))
			{
				if (current_render_mode == AORenderMode::eAO_Applied)
					current_render_mode = AORenderMode::eAO_Monochrome;
				else
					current_render_mode = AORenderMode::eAO_Applied;
			}
		}
		ImGui::End();

		auto & transform_manager = Singleton<Component::TransformManager>();

		if (ImGui::Begin("Scene Graph"))
		{
			transform_manager.DisplaySceneGraph();
		}
		ImGui::End();

		auto const & editor_scene_graph_data = transform_manager.GetEditorSceneGraphData();

		if (ImGui::Begin("Component Editor"))
		{
			if (editor_scene_graph_data.selected_entities.size() == 1)
			{
				Engine::ECS::Entity const selected_entity = *editor_scene_graph_data.selected_entities.begin();
				for (auto manager : Engine::ECS::ICompManager::GetRegisteredComponentManagers())
				{
					manager->EditComponent(selected_entity);
				}
			}
			else
			{
				ImGui::Text("No entity selected");
			}
			if (!editor_scene_graph_data.selected_entities.empty())
			{
				ImGui::Button("Create Component");
				if (ImGui::BeginPopupContextItem("CreatableComponentList", ImGuiPopupFlags_MouseButtonLeft))
				{
					static std::string s_search_input;
					if (ImGui::IsWindowAppearing())
						s_search_input.clear();

					ImGui::InputText("Search", &s_search_input, ImGuiInputTextFlags_CharsNoBlank);
					for (auto manager : Engine::ECS::ICompManager::GetRegisteredComponentManagers())
					{
						bool show_component = s_search_input.empty();
						std::string const comp_name = manager->GetComponentTypeName();
						if (!s_search_input.empty())
						{
							std::string lower_case_search_input = s_search_input;
							for (char& c : lower_case_search_input) c = std::tolower(c);
							std::string lower_case_comp_name;
							for (char& c : lower_case_comp_name) c = std::tolower(c);
							size_t result = lower_case_comp_name.find(lower_case_search_input);
							if (result != std::string::npos)
								show_component = true;
						}

						if (show_component && ImGui::Selectable(comp_name.c_str()))
						{
							for (auto entity : editor_scene_graph_data.selected_entities)
								manager->CreateComponent(entity);
						}
					}
					ImGui::EndPopup();
				}
			}
		}
		ImGui::End();

		system_resource_manager.EditorDisplay();

	}

	void GameplayLogic()
	{
		using namespace Engine::Managers;
		using namespace Component;
		using button_state = InputManager::button_state;

		float const DT = 1.0f / 60.0f;

		Singleton<Component::PlayerControllerManager>().Update(DT);

		// Get rigidbody and detect if ray from camera intersects body.
		// If so, impart a force on object at relative position.

		static float holddown_timer = 0.0f;
		auto& input_mgr = Singleton<InputManager>();
		auto& rb_mgr = Singleton<RigidBodyManager>();
		auto& collider_mgr = Singleton<ColliderManager>();

		button_state mb_state = input_mgr.GetKeyboardButtonState(SDL_SCANCODE_F);
		if (mb_state == button_state::eDown)
			holddown_timer += DT;
		else if (mb_state == button_state::eUnpressed)
		{
			glm::ivec2 const mouse_window_pos = input_mgr.GetMousePos();
			glm::vec2 const normalized_mouse_pos = glm::vec2(mouse_window_pos) / glm::vec2(Singleton<Engine::sdl_manager>().get_window_size());
			// Flip Y-axis since mouse window position starts in top-left rather than bottom-left.
			glm::vec4 const mouse_ndc_pos = glm::vec4(
				((2.0f * normalized_mouse_pos) - 1.0f) * glm::vec2(1.0f, -1.0f),
				-1.0f,
				1.0f
			);

			Engine::ECS::Entity camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
			Camera camera_component = camera_entity.GetComponent<Camera>();
			Transform camera_transform = camera_entity.GetComponent<Transform>();

			Engine::Graphics::camera_data const cam_data = camera_component.GetCameraData();

			glm::mat4 const perspective_mat = cam_data.get_perspective_matrix();
			glm::mat4 const view_mat = camera_transform.ComputeWorldTransform().GetInverse().GetMatrix();
			glm::mat4 const inv_vp = glm::inverse(perspective_mat * view_mat);

			// Define start and end of ray

			glm::mat2x4 const ray_ndc_points(
				mouse_ndc_pos, 
				glm::vec4(mouse_ndc_pos.x, mouse_ndc_pos.y, 1.0f, 1.0f)
			);
			glm::mat2x4 const ray_world_points = inv_vp* ray_ndc_points;
			glm::vec3 const & ray_world_start = ray_world_points[0] / ray_world_points[0].w;
			glm::vec3 const & ray_world_end = ray_world_points[1] / ray_world_points[1].w;
			glm::vec3 const& ray_direction = glm::normalize(ray_world_end - ray_world_start);

			Engine::Physics::ray camera_ray;
			camera_ray.origin = ray_world_start;
			camera_ray.dir = glm::normalize(ray_world_end - ray_world_start);

			auto const & rb_data = rb_mgr.m_rigidbodies_data;
			auto & col_data = collider_mgr.m_data;
			for (auto & [entity, debug_render_instance] : col_data.m_entity_map)
			{
				using namespace Engine::Physics;

				convex_hull_handle const ch_handle = debug_render_instance.m_collider_resource.Handle();

				RigidBody rb_comp = entity.GetComponent<RigidBody>();
				auto ch_info = Singleton<ConvexHullManager>().GetConvexHullInfo(ch_handle);
				if (ch_info && rb_comp.IsValid())
				{
					Transform rb_transform = entity.GetComponent<Transform>();
					Engine::Math::transform3D const rb_world_transform = rb_transform.ComputeWorldTransform();
					glm::vec3 const rb_world_position = rb_world_transform.position;

					intersection_result result = intersect_ray_half_edge_data_structure(camera_ray, ch_info->m_data, rb_world_transform);

					if (result.t >= 0.0f)
					{
						rb_mgr.ApplyForce(
							entity,
							ray_direction * std::clamp(holddown_timer * 500.0f, 0.0f, 1000.0f),
							 (camera_ray.origin + result.t * camera_ray.dir) - rb_world_position
						);
						debug_render_instance.m_highlight_face_index = result.face_index;
					}
				}
			}

			holddown_timer = 0.0f;
		}
	}

	void Update()
	{
		if(s_scene_reset)
			ResetSceneData();

		using sdl_manager = Engine::sdl_manager;

		auto& res_mgr = Singleton<Engine::Graphics::ResourceManager>();
		auto& input_manager = Singleton<Engine::Managers::InputManager>();

		DummyEditorRender();

		using button_state = Engine::Managers::InputManager::button_state;

		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_F5) == button_state::ePress)
			Singleton<Engine::Graphics::ResourceManager>().RefreshShaders();

		if ((input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_LSHIFT) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_R) == button_state::ePress))
			Singleton<Engine::sdl_manager>().m_want_restart = true;

		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_Q) == button_state::ePress)
			Singleton<Engine::sdl_manager>().m_want_quit = true;

		frame_counter++;

		GameplayLogic();
		Singleton<Component::ColliderManager>().TestColliderIntersections();

		s_time += 1.0 / 60.0;
		GraphicsPipelineRender();

		if (s_bool_save_screenshot)
			SaveScreenShot(s_saved_screenshot_name.c_str());

		if ((input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_R) == button_state::ePress))
		{
			Singleton<Component::SceneEntityComponentManager>().DestroyAllSceneEntities();
			s_scene_reset = true;
		}
	}

	void Shutdown()
	{
		ShutdownGraphicsPipelineRender();

		shutdown_render_common();

		ShutdownSandboxComponentManagers();
	}
}