#include <engine/Graphics/manager.h>

namespace Engine {
namespace Graphics {

	uint32_t load_obj(fs::path const & _filepath);
	void unload_obj_mesh(uint32_t _handle);

}
}