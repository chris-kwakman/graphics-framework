#ifndef SDL_WINDOW_H
#define SDL_WINDOW_H

#include <SDL2/SDL.h>
#include <glm/vec2.hpp>
#include <string>

namespace Engine
{

	class sdl_manager
	{
		bool			m_file_was_dropped = false;
		std::string		m_dropped_file_dir;

	public:

		SDL_Window*		m_window = nullptr;
		SDL_Surface*	m_surface = nullptr;
		SDL_GLContext	m_gl_context = 0;

		bool m_want_quit = false;
		bool m_want_restart = false;

		bool setup_volumetric_fog(glm::uvec2 _window_size);
		void update();
		void shutdown_volumetric_fog();

		glm::uvec2 get_window_size() const;

		bool is_file_dropped() const { return m_file_was_dropped; }
		std::string get_dropped_file() {
			std::string return_file = "";
			if (m_file_was_dropped)
			{
				m_file_was_dropped = false;
				return_file = m_dropped_file_dir;
				m_dropped_file_dir.clear();
			}
			return return_file;
		}

		void set_gl_debug_state(bool _state);
	};

}

#endif // !SDL_H
