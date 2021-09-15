#ifndef ENGINE_EDITOR_H
#define ENGINE_EDITOR_H

#include <SDL2/SDL_events.h>
#include <ImGui/imgui.h>

namespace Engine {
namespace Editor {

	class Editor
	{
	public:

		void Initialise();
		void ProcessSDLEvent(SDL_Event* _event);
		void NewFrame();
		void Render();
		void Shutdown();

	private:

	};

}
}

#endif // !ENGINE_EDITOR_H
