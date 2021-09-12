#ifndef ENGINE_MANAGERS_INPUT_H
#define ENGINE_MANAGERS_INPUT_H

#include <SDL2/SDL_scancode.h>

namespace Engine {
namespace Managers {

	class InputManager
	{
	public:

		InputManager();

		enum class button_state : Uint8 {eUp = 0, ePress = 1, eUnpressed = 2, eDown = 3};

		void UpdateKeyboardState(bool _application_taking_keyboard_input = true);
		button_state GetKeyboardButtonState(SDL_Scancode _scancode) const;

	private:

		button_state m_button_state_arr[SDL_NUM_SCANCODES];
	};

}
}
#endif // !ENGINE_MANAGERS_INPUT_H
