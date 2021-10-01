#ifndef ENGINE_GRAPHICS_LIGHT_H
#define ENGINE_GRAPHICS_LIGHT_H

#include <glm/vec3.hpp>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera_data.h>

namespace Engine {
namespace Graphics {

	struct light
	{
		glm::vec3 m_pos;
		glm::vec3 m_color;
		float	  m_radius;
	};

	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;

	void RenderLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform,
		light const* _lights, unsigned int _light_count
	);

}
}

#endif // !ENGINE_GRAPHICS_LIGHT_H
