#ifndef ENGINE_MANAGERS_INPUT_H
#define ENGINE_MANAGERS_INPUT_H

#include <SDL2/SDL_scancode.h>
#include <glm/vec2.hpp>

namespace Engine {
namespace Managers {

	class InputManager
	{
	public:

		InputManager();

		enum class button_state : Uint8 {eUp = 0, ePress = 1, eUnpressed = 2, eDown = 3};

		void UpdateMouseState(bool _application_taking_mouse_input = true);
		void UpdateKeyboardState(bool _application_taking_keyboard_input = true);
		button_state GetKeyboardButtonState(SDL_Scancode _scancode) const;
		button_state GetMouseButtonState(unsigned int _mouse_button) const;

		glm::ivec2	GetMousePos() const;
		glm::ivec2	GetMouseDelta() const;

	private:

		button_state m_keyboard_state_arr[SDL_NUM_SCANCODES];
		button_state m_mouse_state_arr[3];

		glm::ivec2	m_mouse_pos_prev;
		glm::ivec2	m_mouse_pos_curr;
	};

}
}
#endif // !ENGINE_MANAGERS_INPUT_H
