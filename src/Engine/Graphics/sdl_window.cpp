#include "sdl_window.h"
#include <GL/glew.h>
#include <Engine/Utils/singleton.h>
#include <Engine/Managers/input.h>

namespace Engine
{
	bool sdl_manager::setup(glm::uvec2 _window_size)
	{
		/*
		* Initialize SDL
		*/

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			printf("SDL did not initialize. SDL_Error: %s\n", SDL_GetError());
			return false;
		}
		else
		{
			m_window = SDL_CreateWindow("CS562 Chris Kwakman", SDL_WINDOWPOS_UNDEFINED, 30, _window_size.x, _window_size.y, SDL_WINDOW_OPENGL);
			if (m_window == nullptr)
			{
				printf("Failed to create window. SDL_Error: %s\n", SDL_GetError());
			}
			else
			{
				m_surface = SDL_GetWindowSurface(m_window);

				// Disable deprecated functions in OpenGL.
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
				SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);	// Enable double-buffering

				m_gl_context = SDL_GL_CreateContext(m_window);
				if (!m_gl_context)
				{
					printf("SDL_GL_CreateContext error: %s\n", SDL_GetError());
					return false;
				}

				GLint glMajorVersion{ 0 };
				GLint glMinorVersion{ 0 };
				glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersion);
				glGetIntegerv(GL_MINOR_VERSION, &glMinorVersion);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajorVersion);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinorVersion);

				glewExperimental = true;
				if (glewInit() != GLEW_OK)
				{
					printf("GLEW Error: Failed to initialize\n");
					return false;
				}

				SDL_SetWindowResizable(m_window, SDL_TRUE);
				SDL_GL_SetSwapInterval(-1);
			}

			return true;
		}
	}

	void sdl_manager::update()
	{
		SDL_Event event;
		Singleton<Engine::Managers::InputManager>().UpdateKeyboardState();
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:	
				m_want_quit = true; 
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE)
					m_want_quit = true;
				break;
			}
		}
	}

	void sdl_manager::shutdown()
	{
		if (m_window)
		{
			SDL_DestroyWindow(m_window);
			m_window = nullptr;
		}

		if (m_gl_context != NULL)
		{
			SDL_GL_DeleteContext(m_gl_context);
			m_gl_context = NULL;
		}

		SDL_Quit();
	}
}