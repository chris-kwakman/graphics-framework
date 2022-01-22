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

#include "Demo/sandbox.h"
#include "Demo/Components/SandboxCompManager.h"

#include <thread>
#include <chrono>
#include <filesystem>
#include <iostream>

#undef main

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

#define MAX_FRAMERATE 60

using ms = std::chrono::milliseconds;
static ms frametime = ms(0);
static ms curr_frame_duration = ms(0);
static ms const max_frametime(1000 / MAX_FRAMERATE);

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

		sdl_manager.set_gl_debug_state(false);

		Singleton<Engine::Editor::Editor>().Render();
		Singleton<Engine::ECS::EntityManager>().FreeQueuedEntities();

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


	
	Engine::sdl_manager& sdl_manager = Singleton<Engine::sdl_manager>();
	if (sdl_manager.setup_volumetric_fog(glm::uvec2(SCREEN_WIDTH, SCREEN_HEIGHT)))
	{
		sdl_manager.m_want_restart = true;
		while (sdl_manager.m_want_restart)
		{
			Component::InitializeEngineComponentManagers();
			Component::InitializeSandboxComponentManagers();

			sdl_manager.m_want_restart = false;
			Singleton<Engine::Editor::Editor>().Initialise();
			Singleton<Engine::ECS::EntityManager>().Reset();
			Singleton<Engine::Graphics::ResourceManager>().Reset();

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