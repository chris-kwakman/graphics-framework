#include "sandbox.h"

#include <Sandbox/lighting_pass.h>
#include <Sandbox/render_common.h>

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
#include <Engine/Components/Renderable.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Light.h>

#include <STB/stb_image_write.h>

#include "LoadScene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GLM/gtx/quaternion.hpp>

#include <ImGui/misc/cpp/imgui_stdlib.h>

#include <string>
#include <math.h>
#include <iostream>
#include <iomanip>

# define MATH_PI           3.14159265358979323846f

Engine::Graphics::ResourceManager::texture_handle		s_display_gbuffer_texture = 0;
// Lighting data
static glm::vec3	s_ambient_color = glm::vec3(1);
static float		s_exposure = 1.0f;
static float		s_gamma_correction_factor = 1.1f;
static float		s_shininess_mult_factor = 100.0f;
// Blur data
static bool			s_bloom_enabled = false;
static glm::vec3	s_bloom_treshhold_color(0.2126f, 0.7152f, 0.0722f);
static unsigned int	s_bloom_blur_count = 5;
static bool			s_render_infinite_grid = false;

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
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
		stbi_flip_vertically_on_write(true);
		stbi_write_png(_filename, width, height, 4, imageData.data(), 0);
	}

	using framebuffer_handle = Engine::Graphics::ResourceManager::framebuffer_handle;
	using texture_handle = Engine::Graphics::ResourceManager::texture_handle;
	static texture_handle s_texture_white = 0;

	static framebuffer_handle s_framebuffer_gbuffer, s_framebuffer_lighting, s_framebuffer_bloom[2], s_framebuffer_shadow;

	static glm::vec4 s_clear_color{ 0.0f,0.0f,0.0f,0.0f };

	texture_handle
		s_fb_texture_depth,
		s_fb_texture_base_color,
		s_fb_texture_normal,
		s_fb_texture_metallic_roughness,
		s_fb_texture_light_color,
		s_fb_texture_luminance,
		s_fb_texture_bloom_pingpong[2],
		s_fb_texture_shadow
	;

	static std::vector<texture_handle> g_gbuffer_textures;

	using Entity = Engine::ECS::Entity;

	void setup_shaders()
	{
		Engine::Graphics::ResourceManager& system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		using shader_handle = Engine::Graphics::ResourceManager::shader_handle;
		using shader_program_handle = Engine::Graphics::ResourceManager::shader_program_handle;

		std::vector<std::filesystem::path> shader_paths = {
			"data/shaders/infinite_grid.vert",
			"data/shaders/infinite_grid.frag",
			"data/shaders/default.vert",
			"data/shaders/deferred.frag",
			"data/shaders/display_framebuffer.vert",
			"data/shaders/display_framebuffer_plain.frag",
			"data/shaders/display_framebuffer_ambient_light.frag",
			"data/shaders/phong.frag",
			"data/shaders/bloom.frag",
			"data/shaders/postprocessing.frag",
			"data/shaders/dirlight_shadowmap.vert",
			"data/shaders/dirlight_shadowmap.frag",
			"data/shaders/write_csm.vert",
			"data/shaders/write_csm.frag"
		};
		std::vector<Engine::Graphics::ResourceManager::shader_handle> output_shader_handles;
		output_shader_handles = system_resource_manager.LoadShaders(shader_paths);

		using program_shader_path_list = std::vector<std::filesystem::path>;

		program_shader_path_list const draw_infinite_grid_shaders = { "data/shaders/infinite_grid.vert", "data/shaders/infinite_grid.frag" };
		program_shader_path_list const draw_gbuffer_shaders = { "data/shaders/default.vert", "data/shaders/deferred.frag" };
		program_shader_path_list const draw_framebuffer_plain_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_plain.frag" };
		program_shader_path_list const draw_framebuffer_ambient_light = { "data/shaders/display_framebuffer.vert", "data/shaders/display_framebuffer_ambient_light.frag" };
		program_shader_path_list const draw_phong_shaders = { "data/shaders/default.vert", "data/shaders/phong.frag" };
		program_shader_path_list const process_bloom_blur_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/bloom.frag" };
		program_shader_path_list const draw_postprocessing_shaders = { "data/shaders/display_framebuffer.vert", "data/shaders/postprocessing.frag" };
		program_shader_path_list const directional_light_shaders = { "data/shaders/dirlight_shadowmap.vert", "data/shaders/dirlight_shadowmap.frag" };
		program_shader_path_list const write_csm_shaders = { "data/shaders/write_csm.vert", "data/shaders/write_csm.frag" };

		system_resource_manager.LoadShaderProgram("draw_infinite_grid", draw_infinite_grid_shaders);
		system_resource_manager.LoadShaderProgram("draw_gbuffer", draw_gbuffer_shaders);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_plain", draw_framebuffer_plain_shaders);
		system_resource_manager.LoadShaderProgram("draw_framebuffer_ambient_light", draw_framebuffer_ambient_light);
		system_resource_manager.LoadShaderProgram("draw_phong", draw_phong_shaders);
		system_resource_manager.LoadShaderProgram("process_bloom_blur", process_bloom_blur_shaders);
		system_resource_manager.LoadShaderProgram("postprocessing", draw_postprocessing_shaders);
		system_resource_manager.LoadShaderProgram("dirlight_shadowmap", directional_light_shaders);
		system_resource_manager.LoadShaderProgram("write_csm", write_csm_shaders);
	}

	glm::uvec2 s_bloom_texture_size;

	void setup_framebuffer()
	{
		Engine::Graphics::ResourceManager& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		glm::uvec2 const surface_size(surface->w, surface->h);

		// Create framebuffer & textures.
		s_framebuffer_gbuffer = resource_manager.CreateFramebuffer();
		s_framebuffer_lighting = resource_manager.CreateFramebuffer();
		s_framebuffer_shadow = resource_manager.CreateFramebuffer();
		for (unsigned int i = 0; i < 2; ++i)
			s_framebuffer_bloom[i] = resource_manager.CreateFramebuffer();
		

		s_fb_texture_depth = resource_manager.CreateTexture("FB Depth Texture");
		s_fb_texture_base_color = resource_manager.CreateTexture("FB Base Color Texture");
		s_fb_texture_normal = resource_manager.CreateTexture("FB Normal Texture");
		s_fb_texture_metallic_roughness = resource_manager.CreateTexture("FB Metallic Roughness Texture");
		s_fb_texture_light_color = resource_manager.CreateTexture("FB Light Color");
		s_fb_texture_luminance = resource_manager.CreateTexture("FB Luminance");
		s_fb_texture_shadow = resource_manager.CreateTexture("FB Shadow Map");
		for (unsigned int i = 0; i < 2; ++i)
			s_fb_texture_bloom_pingpong[i] = resource_manager.CreateTexture("FB Bloom Pingpong");
		s_texture_white = resource_manager.CreateTexture("White Texture");

		s_display_gbuffer_texture = s_fb_texture_base_color;

		//resource_manager.SpecifyAndUploadTexture2D(s_fb_texture_depth, GL_DEPTH_COMPONENT, surface_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		using tex_params = Engine::Graphics::ResourceManager::texture_parameters;
		tex_params default_fb_texture_params;
		default_fb_texture_params.m_wrap_s = GL_CLAMP_TO_EDGE;
		default_fb_texture_params.m_wrap_t = GL_CLAMP_TO_EDGE;
		default_fb_texture_params.m_min_filter = GL_LINEAR;
		default_fb_texture_params.m_mag_filter = GL_LINEAR;
		resource_manager.AllocateTextureStorage2D(s_fb_texture_depth, GL_DEPTH_COMPONENT32F, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_base_color, GL_RGB8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_metallic_roughness, GL_RGB8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_normal, GL_RGB8, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_light_color, GL_RGB16F, surface_size, default_fb_texture_params);
		resource_manager.AllocateTextureStorage2D(s_fb_texture_shadow, GL_R16F, surface_size, default_fb_texture_params);
		tex_params luminance_params = default_fb_texture_params;
		luminance_params.m_min_filter = GL_LINEAR_MIPMAP_LINEAR;
		resource_manager.AllocateTextureStorage2D(s_fb_texture_luminance, GL_RGB16F, surface_size, luminance_params,3);
		s_bloom_texture_size = surface_size / glm::uvec2(4);
		for (unsigned int i = 0; i < 2; ++i)
		{
			// TODO: Half size of bloom blur textures
			resource_manager.AllocateTextureStorage2D(s_fb_texture_bloom_pingpong[i], GL_RGB16F, s_bloom_texture_size, default_fb_texture_params);
		}
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

		g_gbuffer_textures = {
			s_fb_texture_base_color,
			s_fb_texture_depth,
			s_fb_texture_normal,
			s_fb_texture_metallic_roughness,
			s_fb_texture_light_color,
			s_fb_texture_luminance,
			s_fb_texture_shadow
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

	static bool s_reset_editor_camera_data = false;
	void ResetEditorCameraData()
	{
		s_reset_editor_camera_data = false;

		Entity& editor_camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
		editor_camera_entity = Singleton<Component::CameraManager>().AllCameras().begin()->first;
		s_camera_default_transform = editor_camera_entity.GetComponent<Component::Transform>().GetLocalTransform();
	}

	bool Initialize(int argc, char* argv[])
	{
		InitializeSandboxComponentManagers();

		Engine::Graphics::ResourceManager & system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		system_resource_manager.Reset();

		setup_shaders();
		setup_framebuffer();
		create_framebuffer_triangle();

		// Load glTF model Sponza by default, other if specified in commandline argument.
		if (argc >= 2)
			s_scene_loaded = argv[1];
		else
			s_scene_loaded = "data/scenes/sceneShadow.json";
		LoadScene(LoadJSON(s_scene_loaded.c_str()), s_scene_loaded.c_str());

		system_resource_manager.ImportModel_GLTF("data/gltf/Sphere.gltf");

		if (argc >= 3)
		{
			s_bool_save_screenshot = true;
			s_saved_screenshot_name = std::string(argv[2]);
			// Request quit right away.
			Singleton<Engine::sdl_manager>().m_want_quit = true;
		}

		//// Create default light in scene
		//Entity light_entity;
		//Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&light_entity, 1);
		//Component::Transform light_transform_comp = Singleton<Component::TransformManager>().Create(light_entity);
		//light_transform_comp.SetLocalPosition(glm::vec3(0.0f, 50.0f, 0.0f));
		//Component::PointLight light_comp = Singleton<Component::PointLightManager>().Create(light_entity);
		//light_comp.SetRadius(500.0f);

		//// Create editor camera
		//Singleton<Engine::ECS::EntityManager>().EntityCreationRequest(&s_camera_entity, 1);
		//auto camera_transform_comp = Component::Transform::GetManager().Create(s_camera_entity);
		//camera_transform_comp.SetLocalPosition(glm::vec3(0.0f, 10.0f, 0.0f));
		//Component::Camera::GetManager().Create(s_camera_entity);

		s_reset_editor_camera_data = true;

		GfxCall(glDepthRange(-1.0f, 1.0f));

		return true;
	}

	unsigned int frame_counter = 0;
	float const CAM_MOVE_SPEED = 20.0f;
	float const CAM_MOV_SHIFT_MULT = 4.0f;
	float const CAM_ROTATE_SPEED = MATH_PI * 0.75f;
	float const MOUSE_SENSITIVITY = 0.05f;
	bool camera_invert = false;

	void control_camera()
	{
		auto& input_manager = Singleton<Engine::Managers::InputManager>();
		using button_state = Engine::Managers::InputManager::button_state;

		Engine::ECS::Entity camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;

		float const DT = (1.0f / 60.0f);

		float accum_rotate_horizontal = 0.0f, accum_rotate_vertical = 0.0f;

		button_state rotate_up, rotate_down, rotate_left, rotate_right;
		rotate_up = input_manager.GetKeyboardButtonState(SDL_SCANCODE_UP);
		rotate_down = input_manager.GetKeyboardButtonState(SDL_SCANCODE_DOWN);
		rotate_left = input_manager.GetKeyboardButtonState(SDL_SCANCODE_LEFT);
		rotate_right = input_manager.GetKeyboardButtonState(SDL_SCANCODE_RIGHT);
		// Arrow key control rotation accumulation
		accum_rotate_horizontal += 1.0f * (rotate_left == button_state::eDown);
		accum_rotate_horizontal -= 1.0f * (rotate_right == button_state::eDown);
		accum_rotate_vertical += 1.0f * (rotate_up == button_state::eDown);
		accum_rotate_vertical -= 1.0f * (rotate_down == button_state::eDown);

		// Mouse button control rotation accumulation
		button_state left_mouse_down = input_manager.GetMouseButtonState(0);
		if (left_mouse_down == button_state::eDown)
		{
			glm::ivec2 const mouse_delta = input_manager.GetMouseDelta();
			glm::vec2 mouse_rotation_delta = MOUSE_SENSITIVITY * glm::vec2(mouse_delta);
			accum_rotate_horizontal -= mouse_rotation_delta.x;
			accum_rotate_vertical -= mouse_rotation_delta.y;
			if (camera_invert)
			{
				accum_rotate_horizontal *= -1.0f;
				accum_rotate_vertical *= -1.0f;
			}
		}

		glm::quat const quat_identity(1.0f, 0.0f, 0.0f, 0.0f);
		glm::quat quat_rotate_around_y = glm::rotate(
			quat_identity,
			accum_rotate_horizontal * CAM_ROTATE_SPEED * DT, 
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		auto camera_transform_comp = camera_entity.GetComponent<Component::Transform>();
		auto cam_transform = camera_transform_comp.GetLocalTransform();
		cam_transform.quaternion = quat_rotate_around_y * cam_transform.quaternion;

		glm::vec3 const cam_dir = glm::rotate(cam_transform.quaternion, glm::vec3(0.0f, 0.0f, -1.0f));
		// Project camera direction vector onto horizontal plane & normalize.
		glm::vec3 const proj_cam_dir(cam_dir.x, 0.0f, cam_dir.z);
		glm::vec3 const perp_proj_cam_dir = glm::normalize(glm::cross(proj_cam_dir, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::quat const quat_rotate_around_right_vector = glm::rotate(
			quat_identity,
			accum_rotate_vertical * CAM_ROTATE_SPEED * DT, 
			perp_proj_cam_dir
		);
		cam_transform.quaternion = quat_rotate_around_right_vector * cam_transform.quaternion;

		float accum_move_forward_backward = 0.0f, accum_move_left_right = 0.0f, accum_move_up_down = 0.0f;

		button_state move_left, move_right, move_forward, move_backward, move_up, move_down;
		move_left = input_manager.GetKeyboardButtonState(SDL_SCANCODE_A);
		move_right = input_manager.GetKeyboardButtonState(SDL_SCANCODE_D);
		move_forward = input_manager.GetKeyboardButtonState(SDL_SCANCODE_W);
		move_backward = input_manager.GetKeyboardButtonState(SDL_SCANCODE_S);
		move_up = input_manager.GetKeyboardButtonState(SDL_SCANCODE_Q);
		move_down = input_manager.GetKeyboardButtonState(SDL_SCANCODE_E);

		accum_move_left_right += 1.0f * (move_right == button_state::eDown);
		accum_move_left_right -= 1.0f * (move_left == button_state::eDown);
		accum_move_forward_backward += 1.0f * (move_forward == button_state::eDown);
		accum_move_forward_backward -= 1.0f * (move_backward == button_state::eDown);
		accum_move_up_down += 1.0f * (move_up == button_state::eDown);
		accum_move_up_down -= 1.0f * (move_down == button_state::eDown);

		float const move_speed_mult = input_manager.GetKeyboardButtonState(SDL_SCANCODE_LSHIFT) == button_state::eDown ?
			CAM_MOV_SHIFT_MULT : 1.0f;

		cam_transform.position.y += DT * CAM_MOVE_SPEED * move_speed_mult * accum_move_up_down;
		cam_transform.position += DT * CAM_MOVE_SPEED * move_speed_mult * accum_move_forward_backward * cam_dir;
		cam_transform.position += DT * CAM_MOVE_SPEED * move_speed_mult * accum_move_left_right * perp_proj_cam_dir;

		camera_transform_comp.SetLocalTransform(cam_transform);

		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_V) == button_state::eDown)
			camera_transform_comp.SetLocalTransform(s_camera_default_transform);

		// Set camera aspect ratio every frame
		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		float const ar = (float)surface->w / (float)surface->h;
		auto camera_entity_component = camera_entity.GetComponent<Component::Camera>();
		camera_entity_component.SetAspectRatio(ar);
	}

	bool show_demo_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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
				(void*)texture_info.m_gl_source_id,
				ImVec2(content_region_avail.x, content_region_avail.x * image_ar),
				ImVec2(0, 1), ImVec2(1, 0)
			);
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
							std::string lower_case_search_input;
							std::transform(
								s_search_input.begin(), s_search_input.end(), 
								std::back_inserter(lower_case_search_input), std::tolower
							);
							std::string lower_case_comp_name;
							std::transform(comp_name.begin(), comp_name.end(),
								std::back_inserter(lower_case_comp_name), std::tolower
							);
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

	void Update()
	{
		ResetEditorCameraData();

		using sdl_manager = Engine::sdl_manager;

		auto& system_resource_manager = Singleton<Engine::Graphics::ResourceManager>();
		auto& input_manager = Singleton<Engine::Managers::InputManager>();

		if (Singleton<sdl_manager>().is_file_dropped())
		{
			std::string const dropped_file_dir = Singleton<sdl_manager>().get_dropped_file();
			system_resource_manager.ImportModel_GLTF(dropped_file_dir.c_str());
		}
		

		///////////////////////////////////////////
		//		Scene Rendering Pipeline
		///////////////////////////////////////////


		DummyEditorRender();

		using button_state = Engine::Managers::InputManager::button_state;

		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_F5) == button_state::ePress)
			Singleton<Engine::Graphics::ResourceManager>().RefreshShaders();

		if ((input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_LSHIFT) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_R) == button_state::ePress))
			Singleton<Engine::sdl_manager>().m_want_restart = true;

		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_Q) == button_state::ePress)
			Singleton<Engine::sdl_manager>().m_want_quit = true;

		control_camera();
		frame_counter++;

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		GfxCall(glViewport(0, 0, Singleton<Engine::sdl_manager>().m_surface->w, Singleton<Engine::sdl_manager>().m_surface->h));

		using index_buffer_handle = Engine::Graphics::ResourceManager::buffer_handle;
		using namespace Engine::Graphics;

		Entity const & camera_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;

		auto const cam_transform = camera_entity.GetComponent<Component::Transform>().ComputeWorldTransform();
		glm::vec3 const camera_forward = cam_transform.quaternion * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 const camera_right = glm::normalize(glm::cross(camera_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 const camera_up = glm::normalize(glm::cross(camera_forward, camera_right));

		//s_camera_transform.quaternion = camera.get_lookat_quat();

		using shader_program_handle = Engine::Graphics::ResourceManager::shader_program_handle;

		shader_program_handle const program_draw_gbuffer = system_resource_manager.FindShaderProgram("draw_gbuffer");
		shader_program_handle const program_draw_framebuffer_plain = system_resource_manager.FindShaderProgram("draw_framebuffer_plain");
		shader_program_handle const program_draw_ambient_light = system_resource_manager.FindShaderProgram("draw_framebuffer_ambient_light");
		assert(program_draw_gbuffer != 0);

		auto camera_component = camera_entity.GetComponent<Component::Camera>();

		system_resource_manager.UseProgram(program_draw_gbuffer);

		/*ResourceManager::mesh_handle mesh_to_render = resource_manager.FindMesh("Sponza/Mesh_0");
		if (mesh_to_render == 0)
			mesh_to_render = resource_manager.FindMesh("Sphere/Sphere");*/

		system_resource_manager.BindFramebuffer(s_framebuffer_gbuffer);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		auto window_size = Singleton<Engine::sdl_manager>().get_window_size();
		glViewport(0, 0, window_size.x, window_size.y);

		//// If we cannot render anything, just clear framebuffer so editor can render.
		//if (mesh_to_render == 0)
		//{
		//	glViewport(0, 0, (GLsizei)window_size.x, (GLsizei)window_size.y);

		//	resource_manager.UnbindFramebuffer();
		//	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		//	glEnable(GL_CULL_FACE);
		//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//	glDepthMask(GL_TRUE);
		//	glEnable(GL_DEPTH_TEST);
		//	glDepthFunc(GL_ALWAYS);

		//	return;
		//}

		glm::mat4 const camera_view_matrix = cam_transform.GetInvMatrix();
		auto camera_data = camera_component.GetCameraData();
		glm::mat4 const camera_perspective_matrix = camera_data.is_orthogonal_camera() ? 
			camera_data.get_orthogonal_matrix() : camera_data.get_perspective_matrix();
		glm::mat4 const matrix_vp = camera_perspective_matrix * camera_view_matrix;

		auto all_renderables = Singleton<Component::RenderableManager>().GetAllRenderables();
		for (auto pair : all_renderables)
		{
			Engine::ECS::Entity const renderable_entity = pair.first;
			ResourceManager::mesh_handle const renderable_mesh = pair.second;
			if (renderable_mesh == 0) 
				continue;

			Component::Transform renderable_transform = renderable_entity.GetComponent<Component::Transform>();
			auto model_transform = renderable_transform.ComputeWorldTransform();
			glm::mat4 const mesh_model_matrix = model_transform.GetMatrix();
			glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
			glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

			system_resource_manager.SetBoundProgramUniform(5, matrix_vp * mesh_model_matrix);
			system_resource_manager.SetBoundProgramUniform(6, matrix_t_inv_mv);
			system_resource_manager.SetBoundProgramUniform(9, matrix_vp * mesh_model_matrix);

			auto& mesh_primitives = system_resource_manager.GetMeshPrimitives(renderable_mesh);
			for (unsigned int i = 0; i < mesh_primitives.size(); ++i)
			{
				ResourceManager::mesh_primitive_data primitive = mesh_primitives[i];

				// Set texture slots
				if (primitive.m_material_handle != 0)
				{
					ResourceManager::material_data material = system_resource_manager.GetMaterial(primitive.m_material_handle);

					texture_handle const use_base_color = material.m_pbr_metallic_roughness.m_texture_base_color
						? material.m_pbr_metallic_roughness.m_texture_base_color
						: s_texture_white;
					texture_handle const use_metallic_roughness = material.m_pbr_metallic_roughness.m_texture_metallic_roughness
						? material.m_pbr_metallic_roughness.m_texture_metallic_roughness
						: s_texture_white;

					// TODO: Implement metallic roughness color factors
					activate_texture(use_base_color, 0, 0);
					activate_texture(use_metallic_roughness, 1, 1);
					activate_texture(material.m_texture_normal, 2, 2);
					system_resource_manager.SetBoundProgramUniform(3, material.m_pbr_metallic_roughness.m_base_color_factor);
					/*activate_texture(material.m_texture_occlusion, 3, 3);
					activate_texture(material.m_texture_emissive, 4, 4);*/


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
						glDisable(GL_CULL_FACE);
					else
						glEnable(GL_CULL_FACE);
				}

				GfxCall(glBindVertexArray(primitive.m_vao_gl_id));
				if (primitive.m_index_buffer_handle != 0)
				{
					auto index_buffer_info = system_resource_manager.GetBufferInfo(primitive.m_index_buffer_handle);
					auto ibo_info = system_resource_manager.GetIndexBufferInfo(primitive.m_index_buffer_handle);
					GfxCall(glDrawElements(
						primitive.m_render_mode,
						(GLsizei)primitive.m_index_count,
						ibo_info.m_type,
						(GLvoid*)primitive.m_index_byte_offset
					));
				}
				else
				{
					GfxCall(glDrawArrays(primitive.m_render_mode, 0, primitive.m_vertex_count));
				}
				glBindVertexArray(0);

			
			}

		}

		cascading_shadow_map_data csm_data;
		if (Singleton<Component::DirectionalLightManager>().GetDirectionalLight().IsValid())
		{
			csm_data = Sandbox::RenderDirectionalLightCSM(camera_component.GetCameraData(), cam_transform);
			Sandbox::RenderShadowMapToFrameBuffer(camera_component.Owner(), window_size, csm_data, s_fb_texture_depth, s_framebuffer_shadow);
		}

		// Lighting Stage
		system_resource_manager.BindFramebuffer(s_framebuffer_lighting);
		GLenum const draw_fb_lighting_attachment_01[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		system_resource_manager.DrawFramebuffers(s_framebuffer_lighting, 2, draw_fb_lighting_attachment_01);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(0, 0, window_size.x, window_size.y);
		glDisable(GL_BLEND);

		system_resource_manager.UseProgram(system_resource_manager.FindShaderProgram("draw_phong"));
		system_resource_manager.SetBoundProgramUniform(20, s_gamma_correction_factor);
		system_resource_manager.SetBoundProgramUniform(21, s_shininess_mult_factor);
		system_resource_manager.SetBoundProgramUniform(22, s_bloom_treshhold_color);

		activate_texture(s_fb_texture_depth, 0, 0);
		activate_texture(s_fb_texture_base_color, 1, 1);
		activate_texture(s_fb_texture_metallic_roughness, 2, 2);
		activate_texture(s_fb_texture_normal, 3, 3);

		ResourceManager::mesh_handle sphere_mesh = system_resource_manager.FindMesh("Sphere/Sphere");
		if (sphere_mesh)
		{
			Sandbox::RenderPointLights(sphere_mesh, camera_component.GetCameraData(), cam_transform);
		}

		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glDepthMask(GL_TRUE);
		glCullFace(GL_BACK);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		if (s_ambient_color != glm::vec3(0))
		{
			glEnable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);

			system_resource_manager.UseProgram(program_draw_ambient_light);
			system_resource_manager.SetBoundProgramUniform(10, s_ambient_color);
			activate_texture(s_fb_texture_base_color, 0, 0);

			// Disable drawing to luminance buffer when computing ambient color.
			GLenum const draw_fb_lighting_attachment_0[] = { GL_COLOR_ATTACHMENT0};
			system_resource_manager.DrawFramebuffers(s_framebuffer_lighting, 1, draw_fb_lighting_attachment_0);

			GfxCall(glBindVertexArray(s_gl_tri_vao));
			GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo));
			GfxCall(glDrawElements(
				GL_TRIANGLES,
				3,
				GL_UNSIGNED_BYTE,
				nullptr
			));
		}

		if (s_bloom_enabled)
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glDepthMask(GL_FALSE);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);

			shader_program_handle bloom_blur_program = system_resource_manager.FindShaderProgram("process_bloom_blur");
			system_resource_manager.UseProgram(bloom_blur_program);

			Engine::Graphics::ResourceManager::texture_info luminance_tex_info = system_resource_manager.GetTextureInfo(s_fb_texture_luminance);
			
			glBindTexture(luminance_tex_info.m_target, luminance_tex_info.m_gl_source_id);
			glGenerateMipmap(luminance_tex_info.m_target);

			GfxCall(glBindVertexArray(s_gl_tri_vao));
			GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo));

			bool first_iteration = true;
			for (unsigned int i = 0; i < s_bloom_blur_count; ++i)
			{
				auto use_bloom_input_texture = first_iteration ? s_fb_texture_luminance : s_fb_texture_bloom_pingpong[(i + 1) % 2];
				system_resource_manager.BindFramebuffer(s_framebuffer_bloom[i%2]);
				activate_texture(use_bloom_input_texture, 0, 0);

				int texture_gl_id = system_resource_manager.GetTextureInfo(use_bloom_input_texture).m_gl_source_id;
				glViewport(0, 0, s_bloom_texture_size.x, s_bloom_texture_size.y);

				system_resource_manager.SetBoundProgramUniform(1, (int)((i % 2) == 0));

				GfxCall(glDrawElements(
					GL_TRIANGLES,
					3,
					GL_UNSIGNED_BYTE,
					nullptr
				));
				first_iteration = false;

			}

			glBindVertexArray(0);
		}

		// Render onto final framebuffer
		glViewport(0, 0, (GLsizei)window_size.x, (GLsizei)window_size.y);

		system_resource_manager.UnbindFramebuffer();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glEnable(GL_CULL_FACE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		glDisable(GL_BLEND);

		shader_program_handle const postprocessing_program = system_resource_manager.FindShaderProgram("postprocessing");

		system_resource_manager.UseProgram(postprocessing_program);

		activate_texture(s_fb_texture_light_color, 0, 0);
		activate_texture(s_fb_texture_bloom_pingpong[1], 1, 1);
		activate_texture(s_fb_texture_depth, 2, 2);

		system_resource_manager.SetBoundProgramUniform(10, s_exposure);
		system_resource_manager.SetBoundProgramUniform(11, s_gamma_correction_factor);

		GfxCall(glBindVertexArray(s_gl_tri_vao));
		GfxCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_gl_tri_ibo));
		GfxCall(glDrawElements(
			GL_TRIANGLES,
			3,
			GL_UNSIGNED_BYTE,
			nullptr
		));

		glBindVertexArray(0);

		if (s_render_infinite_grid)
		{
			// Render endless grid
			auto program = system_resource_manager.FindShaderProgram("draw_infinite_grid");
			system_resource_manager.UseProgram(program);
			system_resource_manager.SetBoundProgramUniform(0, cam_transform.GetInvMatrix());
			system_resource_manager.SetBoundProgramUniform(1, camera_component.GetCameraData().get_perspective_matrix());
			system_resource_manager.SetBoundProgramUniform(10, camera_component.GetNearDistance());
			system_resource_manager.SetBoundProgramUniform(11, camera_component.GetFarDistance());

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LEQUAL);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		if (s_bool_save_screenshot)
			SaveScreenShot(s_saved_screenshot_name.c_str());

		if ((input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eDown && input_manager.GetKeyboardButtonState(SDL_SCANCODE_R) == button_state::ePress))
		{
			Singleton<Component::SceneEntityComponentManager>().DestroyAllSceneEntities();
			LoadScene(LoadJSON(s_scene_loaded.c_str()), s_scene_loaded.c_str());

			s_reset_editor_camera_data = true;
		}
	}

	void Shutdown()
	{
		ShutdownSandboxComponentManagers();
	}
}