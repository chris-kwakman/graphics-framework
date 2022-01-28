#include "camera.h"

#include <Engine/Graphics/sdl_window.h>

namespace Component
{

	using camera_data = Engine::Graphics::camera_data;

	bool CameraManager::impl_create(Entity _e)
	{
		camera_data new_camera;

		auto & sdl_manager = Singleton<Engine::sdl_manager>();

		new_camera.m_aspect_ratio = (float)sdl_manager.get_window_size().x / (float)sdl_manager.get_window_size().y;
		new_camera.set_vertical_fov(3.1415f * 0.4f);
		new_camera.m_near = 0.5f;
		new_camera.m_far = 5000.0f;
		m_camera_data_map.emplace(_e, new_camera);

		return true;
	}

	void CameraManager::impl_destroy(Entity const* _entities, unsigned int _count)
	{
		for (unsigned int i = 0; i < _count; ++i)
		{
			Entity const current_entity = _entities[i];
			m_camera_data_map.erase(current_entity);
		}
	}

	bool CameraManager::impl_component_owned_by_entity(Entity _entity) const
	{
		return m_camera_data_map.find(_entity) != m_camera_data_map.end();
	}

	void CameraManager::impl_edit_component(Entity _entity)
	{
		float const MATH_PI = 3.1415f;

		camera_data& entity_camera_data = m_camera_data_map.at(_entity);
		if (ImGui::InputFloat("Aspect Ratio", &entity_camera_data.m_aspect_ratio, 0.01f, 0.5f))
			entity_camera_data.m_aspect_ratio = std::clamp(entity_camera_data.m_aspect_ratio, 0.25f, 4.0f);
		ImGui::SliderFloat("Near Distance", &entity_camera_data.m_near, 0.01f, entity_camera_data.m_far, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat("Far Distance", &entity_camera_data.m_far, entity_camera_data.m_near, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		bool is_orthogonal = entity_camera_data.is_orthogonal_camera();
		ImGui::Checkbox("Orthogonal", &is_orthogonal);
		if (is_orthogonal)
			ImGui::SliderFloat("Orthogonal Width", &entity_camera_data.m_camera_type_value, 0.01f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		else
			ImGui::SliderFloat("Vertical FOV", &entity_camera_data.m_camera_type_value, MATH_PI * 0.1f, MATH_PI * 0.95f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		entity_camera_data.m_is_orthogonal_camera = is_orthogonal;
	}

	Engine::Graphics::camera_data& CameraManager::get_camera_data(Entity _e)
	{
		return m_camera_data_map.at(_e);
	}

	Engine::Graphics::camera_data const & CameraManager::get_camera_data(Entity _e) const
	{
		return m_camera_data_map.at(_e);
	}

	void CameraManager::impl_deserialize_data(nlohmann::json const& _j)
	{
		int const serializer_version = _j["serializer_version"];
		if (serializer_version == 1)
		{
			m_camera_data_map = _j["m_camera_data_map"].get<decltype(m_camera_data_map)>();
		}
	}

	void CameraManager::impl_serialize_data(nlohmann::json& _j) const
	{
		_j["serializer_version"] = 1;

		_j["m_camera_data_map"] = m_camera_data_map;
	}

	void CameraManager::impl_clear()
	{
		m_camera_data_map.clear();
	}


	float Camera::GetVerticalFOV() const
	{
		return GetManager().get_camera_data(m_owner).get_vertical_fov();
	}

	float Camera::GetNearDistance() const
	{
		return GetManager().get_camera_data(m_owner).m_near;
	}

	float Camera::GetFarDistance() const
	{
		return GetManager().get_camera_data(m_owner).m_far;
	}

	float Camera::GetAspectRatio() const
	{
		return GetManager().get_camera_data(m_owner).m_aspect_ratio;
	}

	Engine::Graphics::camera_data Camera::GetCameraData() const
	{
		return GetManager().get_camera_data(m_owner);
	}

	void Camera::SetVerticalFOV(float _value)
	{
		GetManager().get_camera_data(m_owner).set_vertical_fov(_value);
	}

	void Camera::SetNearDistance(float _value)
	{
		GetManager().get_camera_data(m_owner).m_near = _value;
	}

	void Camera::SetFarDistance(float _value)
	{
		GetManager().get_camera_data(m_owner).m_far = _value;
	}

	void Camera::SetAspectRatio(float _value)
	{
		GetManager().get_camera_data(m_owner).m_aspect_ratio = _value;
	}

	void Camera::SetCameraData(Engine::Graphics::camera_data _camera_data)
	{
		GetManager().get_camera_data(m_owner) = _camera_data;
	}

	bool Camera::IsOrthogonal() const
	{
		return GetManager().get_camera_data(m_owner).is_orthogonal_camera();
	}

}