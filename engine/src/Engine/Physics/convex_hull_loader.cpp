#include "convex_hull_loader.h"
#include "convex_hull.h"

#include <fstream>
#include <string>

#include "point_hull.h"

#include <glm/gtc/epsilon.hpp>
#include <engine/Utils/singleton.h>

#include <Engine/Utils/load_obj_data.hpp>

namespace Engine {
namespace Physics {

	uint32_t LoadConvexHull_CS350(fs::path const & _path);

	half_edge_data_structure ConstructConvexHull_OBJ(fs::path const& _path, bool _quickhull)
	{
		std::vector<glm::vec3> vertices;
		std::vector<glm::uvec3> face_vertex_indices;

		Engine::Utils::load_obj_data(_path, &face_vertex_indices, &vertices, nullptr, nullptr);

		if (face_vertex_indices.empty() || _quickhull)
		{
			return construct_convex_hull(
				&vertices.front(), vertices.size()
			);
		}
		else
		{
			try {
				return construct_half_edge_data_structure(
					&vertices.front(), vertices.size(),
					&face_vertex_indices.front(), face_vertex_indices.size()
				);
			}
			catch (...)
			{
				return construct_convex_hull(
					&vertices.front(), vertices.size()
				);
			}
		}

	}

	uint32_t LoadConvexHull(fs::path const& _path)
	{
		if (!fs::exists(_path))
			return 0;

		auto ext = _path.extension();
		if (ext == ".obj")
			return LoadConvexHull_OBJ(_path);
		else if (ext == ".cs350")
			return LoadConvexHull_CS350(_path);
		else
			return 0;
	}

	uint32_t LoadConvexHull_OBJ(fs::path const& _path)
	{
		half_edge_data_structure new_hull = ConstructConvexHull_OBJ(_path, true);

		return Singleton<ConvexHullManager>().RegisterConvexHull(std::move(new_hull), _path.string());
	}


	uint32_t LoadConvexHull_CS350(fs::path const& _path)
	{
		auto vertices = load_point_hull_vertices(_path);

		half_edge_data_structure new_hull = construct_convex_hull(
			&vertices.front(), vertices.size()
		);

		return Singleton<ConvexHullManager>().RegisterConvexHull(std::move(new_hull), _path.string());
	}

	void UnloadConvexHull(uint32_t _handle)
	{
		Singleton<ConvexHullManager>().DeleteConvexHull(_handle);
	}

}
}