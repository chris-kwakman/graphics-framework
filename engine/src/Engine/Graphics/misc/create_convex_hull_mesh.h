#include <engine/Graphics/manager.h>

namespace Engine {
namespace Physics
{
	struct Engine::Physics::convex_hull;
}
namespace Graphics {
namespace Misc
{
	using namespace Engine::Graphics;

	mesh_handle create_convex_hull_mesh(Physics::convex_hull const& _ch);
}
}
}