#pragma once

#include <Engine/Graphics/manager.h>
#include <Engine/Physics/convex_hull.h>

namespace Engine {
namespace Graphics {
namespace Misc
{
	Engine::Graphics::mesh_handle create_convex_hull_mesh(
		Engine::Physics::convex_hull_handle _ch_handle, 
		std::string _name = ""
	);
}
}
}