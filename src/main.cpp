#include <stdio.h>
#include "Engine/Graphics/sdl_window.h"
#include <Engine/Editor/editor.h>
#include "Engine/Utils/singleton.h"
#include "Engine/ECS/entity.h"

#include "Sandbox/sandbox.h"

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

	while (true)
	{
		auto frame_end = std::chrono::high_resolution_clock::now();
		frametime = std::chrono::duration_cast<ms>(frame_end - frame_start);

		if (frametime < max_frametime)
		{
			std::this_thread::sleep_for(ms(1));
			continue;
		}

		frame_start = frame_end;

		// Logic

		Engine::sdl_manager & sdl_manager = Singleton<Engine::sdl_manager>();
		sdl_manager.update();

		char window_title[128];
		snprintf(window_title, sizeof(window_title), "c.kwakman | FPS: %.2f", 1000.0f / (float)frametime.count());
		SDL_SetWindowTitle(sdl_manager.m_window, window_title);

		Singleton<Engine::Editor::Editor>().NewFrame();

		Sandbox::Update();

		Singleton<Engine::Editor::Editor>().Render();
		Singleton<Engine::ECS::EntityManager>().FreeQueuedEntities();

		SDL_GL_SwapWindow(Singleton<Engine::sdl_manager>().m_window);

		if (sdl_manager.m_want_quit)
			break;
	}
}

int main(int argc, char* argv[])
{
	std::string const cwd = std::filesystem::current_path().string();
	printf("Working directory: %s\n", cwd.c_str());

	
	Engine::sdl_manager& sdl_manager = Singleton<Engine::sdl_manager>();
	if (sdl_manager.setup(glm::uvec2(SCREEN_WIDTH, SCREEN_HEIGHT)))
	{
		Singleton<Engine::Editor::Editor>().Initialise();
		Singleton<Engine::ECS::EntityManager>().Reset();
		if (Sandbox::Initialize(argc, argv))
			update_loop();
		else
			std::cout << "Sandbox initialization failed.\n";
		Sandbox::Shutdown();
		Singleton<Engine::Editor::Editor>().Shutdown();
	}

	sdl_manager.shutdown();

	return 0;
}