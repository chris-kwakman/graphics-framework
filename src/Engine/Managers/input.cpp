#include "input.h"
#include <SDL2/SDL_keyboard.h>
#include <string.h>

namespace Engine {
namespace Managers {

	InputManager::InputManager()
	{
		memset(m_button_state_arr, sizeof(m_button_state_arr), 0);
	}

	void InputManager::UpdateKeyboardState(bool _application_taking_keyboard_input)
	{
		Uint8 const * sdl_keyboard_state = SDL_GetKeyboardState(nullptr);
		for (unsigned int i = 0; i < SDL_NUM_SCANCODES; ++i)
		{
			if (((bool)sdl_keyboard_state[i] & _application_taking_keyboard_input) == 0)
				m_button_state_arr[i] = (((Uint8)m_button_state_arr[i] & 1) ? button_state::eUnpressed : button_state::eUp);
			else
				m_button_state_arr[i] = (((Uint8)m_button_state_arr[i] & 1) ? button_state::eDown : button_state::ePress);
		}
	}

	InputManager::button_state InputManager::GetKeyboardButtonState(SDL_Scancode _scancode) const
	{
		return m_button_state_arr[_scancode];
	}

}
}