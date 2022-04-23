#include "input.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_mouse.h>
#include <string.h>

namespace Engine {
namespace Managers {

	InputManager::InputManager()
	{
		memset(m_keyboard_state_arr, sizeof(m_keyboard_state_arr), 0);
		memset(m_mouse_state_arr, sizeof(m_mouse_state_arr), 0);
	}

	void InputManager::UpdateMouseState(bool _application_taking_mouse_input)
	{
		m_mouse_pos_prev = m_mouse_pos_curr;
		uint32_t const sdl_mouse_state = SDL_GetMouseState(&m_mouse_pos_curr.x, &m_mouse_pos_curr.y);
		if (!_application_taking_mouse_input)
			m_mouse_pos_curr = m_mouse_pos_prev;
		for (unsigned int i = 0; i < sizeof(m_mouse_state_arr) / sizeof(button_state); ++i)
		{
			bool button_down = sdl_mouse_state & (1 << i);
			if (button_down & _application_taking_mouse_input)
				m_mouse_state_arr[i] = (((Uint8)m_mouse_state_arr[i] & 1) ? button_state::eDown : button_state::ePress);
			else
				m_mouse_state_arr[i] = (((Uint8)m_mouse_state_arr[i] & 1) ? button_state::eUnpressed : button_state::eUp);
		}
	}

	void InputManager::UpdateKeyboardState(bool _application_taking_keyboard_input)
	{
		Uint8 const * sdl_keyboard_state = SDL_GetKeyboardState(nullptr);
		for (unsigned int i = 0; i < SDL_NUM_SCANCODES; ++i)
		{
			if (((bool)sdl_keyboard_state[i] & _application_taking_keyboard_input) == 0)
				m_keyboard_state_arr[i] = (((Uint8)m_keyboard_state_arr[i] & 1) ? button_state::eUnpressed : button_state::eUp);
			else
				m_keyboard_state_arr[i] = (((Uint8)m_keyboard_state_arr[i] & 1) ? button_state::eDown : button_state::ePress);
		}
	}

	InputManager::button_state InputManager::GetKeyboardButtonState(SDL_Scancode _scancode) const
	{
		return m_keyboard_state_arr[_scancode];
	}

	InputManager::button_state InputManager::GetMouseButtonState(unsigned int _mouse_button) const
	{
		return m_mouse_state_arr[_mouse_button];
	}

	glm::ivec2 InputManager::GetMousePos() const
	{
		return m_mouse_pos_curr;
	}

	glm::ivec2 InputManager::GetMouseDelta() const
	{
		return m_mouse_pos_curr - m_mouse_pos_prev;
	}

}
}