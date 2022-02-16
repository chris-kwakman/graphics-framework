#pragma once

#include <Engine/Utils/filesystem.h>

namespace Engine {
namespace Physics {

	uint32_t LoadConvexHull(fs::path const& _path);
	uint32_t LoadConvexHull_OBJ(fs::path const& _path);

	void UnloadConvexHull(uint32_t _handle);

}
}