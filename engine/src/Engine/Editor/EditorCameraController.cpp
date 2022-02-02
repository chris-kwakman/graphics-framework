#include "EditorCameraController.h"

#include <Engine/Managers/input.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <Engine/Graphics/sdl_window.h>

#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>

namespace Component
{
	void EditorCameraController::SetAsActiveEditorCamera()
	{
		if (IsValid())
			GetManager().m_data.m_active_camera = m_owner;
	}

	void EditorCameraControllerManager::UpdateActiveEditorCamera(float const _dt)
	{
		auto& input_manager = Singleton<Engine::Managers::InputManager>();
		using button_state = Engine::Managers::InputManager::button_state;


		Engine::ECS::Entity const camera_entity = m_data.m_active_camera;
		if (!camera_entity.Alive())
			return;

		auto camera_transform_comp = camera_entity.GetComponent<Component::Transform>();

		manager_data::controller_properties const& cam_props = m_data.m_registered_entities[camera_entity];

		float const DT = (1.0f / 60.0f);

		float accum_rotate_horizontal = 0.0f, accum_rotate_vertical = 0.0f;

		button_state rotate_up, rotate_down, rotate_left, rotate_right;
		rotate_up = input_manager.GetKeyboardButtonState(SDL_SCANCODE_UP);
		rotate_down = input_manager.GetKeyboardButtonState(SDL_SCANCODE_DOWN);
		rotate_left = input_manager.GetKeyboardButtonState(SDL_SCANCODE_LEFT);
		rotate_right = input_manager.GetKeyboardButtonState(SDL_SCANCODE_RIGHT);
		// Arrow key control rotation accumulation
		accum_rotate_horizontal += 1.0f * (rotate_left == button_state::eDown);
		accum_rotate_horizontal -= 1.0f * (rotate_right == button_state::eDown);
		accum_rotate_vertical += 1.0f * (rotate_up == button_state::eDown);
		accum_rotate_vertical -= 1.0f * (rotate_down == button_state::eDown);

		// Mouse button control rotation accumulation
		button_state left_mouse_down = input_manager.GetMouseButtonState(0);
		if (left_mouse_down == button_state::eDown)
		{
			glm::ivec2 const mouse_delta = input_manager.GetMouseDelta();
			glm::vec2 mouse_rotation_delta = cam_props.mouse_sensitivity * glm::vec2(mouse_delta);
			accum_rotate_horizontal -= mouse_rotation_delta.x;
			accum_rotate_vertical -= mouse_rotation_delta.y;
			if (cam_props.inverted)
			{
				accum_rotate_horizontal *= -1.0f;
				accum_rotate_vertical *= -1.0f;
			}
		}

		glm::quat const quat_identity(1.0f, 0.0f, 0.0f, 0.0f);
		glm::quat quat_rotate_around_y = glm::rotate(
			quat_identity,
			accum_rotate_horizontal * cam_props.rotate_speed * DT,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		auto cam_transform = camera_transform_comp.GetLocalTransform();
		cam_transform.rotation = quat_rotate_around_y * cam_transform.rotation;

		glm::vec3 const cam_dir = glm::rotate(cam_transform.rotation, glm::vec3(0.0f, 0.0f, -1.0f));
		// Project camera direction vector onto horizontal plane & normalize.
		glm::vec3 const proj_cam_dir(cam_dir.x, 0.0f, cam_dir.z);
		glm::vec3 const perp_proj_cam_dir = glm::normalize(glm::cross(proj_cam_dir, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::quat const quat_rotate_around_right_vector = glm::rotate(
			quat_identity,
			accum_rotate_vertical * cam_props.rotate_speed * DT,
			perp_proj_cam_dir
		);
		cam_transform.rotation = quat_rotate_around_right_vector * cam_transform.rotation;

		float accum_move_forward_backward = 0.0f, accum_move_left_right = 0.0f, accum_move_up_down = 0.0f;

		button_state move_left, move_right, move_forward, move_backward, move_up, move_down;
		move_left = input_manager.GetKeyboardButtonState(SDL_SCANCODE_A);
		move_right = input_manager.GetKeyboardButtonState(SDL_SCANCODE_D);
		move_forward = input_manager.GetKeyboardButtonState(SDL_SCANCODE_W);
		move_backward = input_manager.GetKeyboardButtonState(SDL_SCANCODE_S);
		move_up = input_manager.GetKeyboardButtonState(SDL_SCANCODE_Q);
		move_down = input_manager.GetKeyboardButtonState(SDL_SCANCODE_E);


		if (input_manager.GetKeyboardButtonState(SDL_SCANCODE_LCTRL) == button_state::eUp)
		{
			accum_move_left_right += 1.0f * (move_right == button_state::eDown);
			accum_move_left_right -= 1.0f * (move_left == button_state::eDown);
			accum_move_forward_backward += 1.0f * (move_forward == button_state::eDown);
			accum_move_forward_backward -= 1.0f * (move_backward == button_state::eDown);
			accum_move_up_down += 1.0f * (move_up == button_state::eDown);
			accum_move_up_down -= 1.0f * (move_down == button_state::eDown);
		}

		float const move_speed_mult = input_manager.GetKeyboardButtonState(SDL_SCANCODE_LSHIFT) == button_state::eDown ?
			(cam_props.fast_speed / cam_props.speed): 1.0f;

		cam_transform.position.y += DT * cam_props.speed * move_speed_mult * accum_move_up_down;
		cam_transform.position += DT * cam_props.speed * move_speed_mult * accum_move_forward_backward * cam_dir;
		cam_transform.position += DT * cam_props.speed * move_speed_mult * accum_move_left_right * perp_proj_cam_dir;

		camera_transform_comp.SetLocalTransform(cam_transform);

		// Set camera aspect ratio every frame
		SDL_Surface const* surface = Singleton<Engine::sdl_manager>().m_surface;
		float const ar = (float)surface->w / (float)surface->h;
		auto camera_entity_component = camera_entity.GetComponent<Component::Camera>();
		if(camera_entity_component.IsValid())
			camera_entity_component.SetAspectRatio(ar);

	}



	const char* EditorCameraControllerManager::GetComponentTypeName() const
	{
		return "Editor Camera Controller";
	}
	void EditorCameraControllerManager::impl_clear()
	{
		m_data = manager_data();
	}
	bool EditorCameraControllerManager::impl_create(Entity _e)
	{
		if (!_e.GetComponent<Component::Camera>().IsValid() || !_e.GetComponent<Component::Transform>().IsValid())
			return false;

		manager_data::controller_properties default_properties;
		default_properties.speed = 5.0f;
		default_properties.fast_speed = 10.0f;
		m_data.m_registered_entities.emplace(_e, default_properties);

		return true;
	}
	void EditorCameraControllerManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (size_t i = 0; i < _count; i++)
		{
			m_data.m_registered_entities.erase(_entities[i]);
			if (m_data.m_active_camera == _entities[i])
				m_data.m_active_camera = Entity::InvalidEntity;
		}
	}
	bool EditorCameraControllerManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_data.m_registered_entities.find(_entity) != m_data.m_registered_entities.end();
	}
	void EditorCameraControllerManager::impl_edit_component(Entity _entity)
	{
		manager_data::controller_properties& properties = m_data.m_registered_entities[_entity];

		if (ImGui::Button("Set As Active Editor Camera"))
			m_data.m_active_camera = _entity;

		ImGui::BeginDisabled(m_data.m_active_camera != _entity);

		ImGui::DragFloat("Speed", &properties.speed, 0.1f, 0.0f, properties.fast_speed, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Fast Speed", &properties.fast_speed, 0.1f, properties.speed, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Rotate Speed", &properties.rotate_speed, 0.1f, 0.1f, 3.1415f * 2.0f, "%.3f");
		ImGui::DragFloat("Mouse Sensitivity", &properties.mouse_sensitivity, 0.01f, 0.01f, 0.2f);

		ImGui::EndDisabled();
	}
	void EditorCameraControllerManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_data = _j["m_data"];
		}
	}
	void EditorCameraControllerManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_data"] = m_data;
	}
}