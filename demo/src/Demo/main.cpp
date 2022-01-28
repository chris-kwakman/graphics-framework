#include <stdio.h>
#include "Engine/Graphics/sdl_window.h"
#include <Engine/Editor/editor.h>
#include "Engine/Utils/singleton.h"
#include "Engine/ECS/entity.h"

#include <Engine/ECS/component_manager.h>
#include <Engine/Components/EngineCompManager.h>
#include <Engine/Graphics/manager.h>

#include <Engine/Components/SkeletonAnimator.h>
#include <Engine/Components/CurveFollower.h>
#include <Engine/Components/Rigidbody.h>

#include "Demo/sandbox.h"
#include "Demo/Components/SandboxCompManager.h"

#include <thread>
#include <chrono>
#include <filesystem>
#include <iostream>

#include <fstream>

#undef main

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

#define MAX_FRAMERATE 60

using ms = std::chrono::milliseconds;
static ms frametime = ms(0);
static ms curr_frame_duration = ms(0);
static ms const max_frametime(1000 / MAX_FRAMERATE);

fs::path const scene_directory("data//scenes//");

static fs::path s_load_scene_at_path;


void load_scene(fs::path _scene_path)
{
	if (_scene_path.extension() != ".scene")
		return;

	std::ifstream scene_file(_scene_path);
	if (scene_file.is_open())
	{
		nlohmann::json scene_json;
		scene_file >> scene_json;
		Engine::Serialisation::DeserialiseScene(scene_json);
	}
}

void menu_bar()
{
	static char file_name_buffer[32];
	fs::directory_iterator const dir_iter(scene_directory);

	bool popup_scene_save = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Scene"))
		{
			popup_scene_save = ImGui::MenuItem("Save Scene");
			

			if (ImGui::BeginMenu("Load Scene"))
			{

				for (auto & entry : dir_iter)
				{
					auto const entry_path = entry.path();
					if (fs::is_regular_file(entry) && entry_path.has_extension() && entry_path.extension() == ".scene")
					{
						if (ImGui::MenuItem(entry_path.filename().string().c_str()))
						{
							Singleton<Engine::sdl_manager>().m_want_restart = true;
							s_load_scene_at_path = entry_path;
						}
					}
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();

	}

	if (popup_scene_save)
	{
		auto& io = ImGui::GetIO();
		ImGui::OpenPopup("Save Scene");
		ImGui::SetNextWindowPos(glm::vec2(io.DisplaySize) * 0.5f, ImGuiCond_Appearing, glm::vec2(0.5f, 0.5f));
		memset(file_name_buffer, 0, sizeof(file_name_buffer));
	}

	glm::vec2 const screen_size = Singleton<Engine::sdl_manager>().get_window_size();

	if (ImGui::BeginPopupContextWindow("Save Scene", ImGuiPopupFlags_None))
	{
		bool pressed_enter = ImGui::InputText("File Name", file_name_buffer, sizeof(file_name_buffer), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SetItemDefaultFocus();
		if (pressed_enter)
		{
			fs::path const const scene_file_name = std::string(file_name_buffer) + ".scene";
			fs::path const const scene_file_path = scene_directory / scene_file_name;

			std::ofstream scene_file(scene_file_path);
			if (scene_file.is_open())
			{
				nlohmann::json scene_json;
				Engine::Serialisation::SerialiseScene(scene_json);
				scene_file << std::setw(4) << scene_json;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void update_loop()
{
	auto frame_start = std::chrono::high_resolution_clock::now();
	frametime = ms(0);

	Engine::sdl_manager& sdl_manager = Singleton<Engine::sdl_manager>();

	while (true)
	{
		// TODO: Replace with proper framerate controller
		auto frame_end = std::chrono::high_resolution_clock::now();
		frametime = std::chrono::duration_cast<ms>(frame_end - frame_start);

		if (frametime < max_frametime)
		{
			std::this_thread::sleep_for(ms(1));
			continue;
		}

		frame_start = frame_end;

		// Logic

		sdl_manager.update();

		char window_title[128];
		snprintf(window_title, sizeof(window_title), "c.kwakman | FPS: %.2f", 1000.0f / (float)frametime.count());
		SDL_SetWindowTitle(sdl_manager.m_window, window_title);

		Singleton<Engine::Editor::Editor>().NewFrame();

		Sandbox::Update();

		//TODO: Use frame rate controller DT
		float const TEMP_DT = 1.0f / 60.0f;
		Singleton<Component::CurveFollowerManager>().UpdateFollowers(TEMP_DT);
		Singleton<Component::SkeletonAnimatorManager>().UpdateAnimatorInstances(TEMP_DT);
		Singleton<Component::RigidBodyManager>().Integrate(TEMP_DT);
		Singleton<Component::RigidBodyManager>().UpdateTransforms();

		sdl_manager.set_gl_debug_state(false);

		Singleton<Engine::ECS::EntityManager>().FreeQueuedEntities();

		menu_bar();
		Singleton<Engine::Editor::Editor>().Render();

		sdl_manager.set_gl_debug_state(true);

		SDL_GL_SwapWindow(Singleton<Engine::sdl_manager>().m_window);

		if (sdl_manager.m_want_quit || sdl_manager.m_want_restart)
			break;
	}
}

void import_default_resources()
{
	Singleton<Engine::Graphics::ResourceManager>().ImportModel_GLTF("data/gltf/Sphere.gltf");
	Singleton<Engine::Graphics::ResourceManager>().ImportModel_GLTF("data/gltf/Box.gltf");
	Singleton<Engine::Graphics::ResourceManager>().ImportModel_GLTF("data/meshes/Cube.gltf");
}

int main(int argc, char* argv[])
{
	std::string const cwd = std::filesystem::current_path().string();
	printf("Working directory: %s\n", cwd.c_str());

	s_load_scene_at_path = argv[1];
	
	Engine::sdl_manager& sdl_manager = Singleton<Engine::sdl_manager>();
	if (sdl_manager.setup_volumetric_fog(glm::uvec2(SCREEN_WIDTH, SCREEN_HEIGHT)))
	{
		sdl_manager.m_want_restart = true;
		while (sdl_manager.m_want_restart)
		{
			sdl_manager.m_want_restart = false;

			Component::InitializeEngineComponentManagers();
			Component::InitializeSandboxComponentManagers();

			Singleton<Engine::Editor::Editor>().Initialise();
			Singleton<Engine::ECS::EntityManager>().Reset();
			Singleton<Engine::Graphics::ResourceManager>().Reset();

			load_scene(s_load_scene_at_path);

			import_default_resources();

			if (Sandbox::Initialize(argc, argv))
				update_loop();
			else
				std::cout << "Sandbox initialization failed.\n";
			Sandbox::Shutdown();

			Singleton<Engine::Editor::Editor>().Shutdown();
			Component::ShutdownEngineComponentManagers();
			Component::ShutdownSandboxComponentManagers();
			Singleton<Engine::Graphics::ResourceManager>().DeleteAllGraphicsResources();
		}
	}

	sdl_manager.shutdown_volumetric_fog();

	return 0;
}