#ifndef ENGINE_EDITOR_H
#define ENGINE_EDITOR_H

#include <SDL2/SDL_events.h>
#include <ImGui/imgui.h>
#include <Engine/ECS/entity.h>

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

		Engine::ECS::Entity EditorCameraEntity;

	private:

	};

}
}

#endif // !ENGINE_EDITOR_H
