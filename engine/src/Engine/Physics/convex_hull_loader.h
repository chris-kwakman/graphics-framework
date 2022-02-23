#pragma once

#include "half_edge.h"
#include <Engine/Utils/filesystem.h>

namespace Engine {
namespace Physics {

	half_edge_data_structure ConstructConvexHull_OBJ(fs::path const& _path, bool _quickhull = false);

	uint32_t LoadConvexHull(fs::path const& _path);
	uint32_t LoadConvexHull_OBJ(fs::path const& _path);

	void UnloadConvexHull(uint32_t _handle);

}
}