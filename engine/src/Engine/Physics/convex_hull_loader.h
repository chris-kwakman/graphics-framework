#pragma once

#include <Engine/Utils/filesystem.h>

namespace Engine {
namespace Physics {

	uint32_t LoadConvexHull_OBJ(fs::path const& _path);
	void UnloadConvexHull_OBJ(uint32_t _handle);

}
}