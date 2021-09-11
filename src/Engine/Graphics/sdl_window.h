#ifndef SDL_WINDOW_H
#define SDL_WINDOW_H

#include <SDL2/SDL.h>
#include <glm/vec2.hpp>

namespace Engine
{

	class sdl_manager
	{
	public:

		SDL_Window*		m_window = nullptr;
		SDL_Surface*	m_surface = nullptr;
		SDL_GLContext	m_gl_context = 0;

		bool m_want_quit = false;

		bool setup(glm::uvec2 _window_size);
		void update();
		void shutdown();
	};

}

#endif // !SDL_H
