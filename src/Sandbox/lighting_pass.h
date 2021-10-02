#ifndef SANDBOX_LIGHTING_PASS_H
#define SANDBOX_LIGHTING_PASS_H

#include <glm/vec3.hpp>
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/camera_data.h>

namespace Sandbox {

	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;

	void RenderLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	);
}



#endif // !SANDBOX_LIGHTING_PASS_H
