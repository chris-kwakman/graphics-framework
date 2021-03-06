#ifndef ENGINE_EDITOR_H
#define ENGINE_EDITOR_H

#include <Engine/ECS/entity.h>
#include <SDL2/SDL_events.h>

#include "my_imgui_config.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>

namespace Engine {
namespace Editor {

	class Editor
	{
	public:

		void Initialise();
		void ProcessSDLEvent(SDL_Event* _event);
		void Update(float const _dt);
		void NewFrame();
		void Render();
		void Shutdown();

		Engine::ECS::Entity EditorCameraEntity;
		const char* ComponentUsingImguizmoWidget = nullptr;

	private:

	};

}
}

#endif // !ENGINE_EDITOR_H
