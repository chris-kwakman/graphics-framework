#include "point_hull.h"
#include <fstream>
#include <engine/Utils/singleton.h>
#include <glm/gtc/epsilon.hpp>

namespace Engine {
namespace Physics {


	std::vector<glm::vec3> load_point_hull_vertices(fs::path const& _path)
	{
		std::fstream in_file(_path);

		size_t tri_count = 0;
		float vertex_coords[9];

		in_file >> tri_count;

		std::vector<glm::vec3> vertices(tri_count * 3);

		for (size_t i = 0; i < tri_count; i++)
		{
			for (size_t j = 0; j < 9; j++)
			{
				in_file >> vertex_coords[j];
			}
			glm::vec3 const* tri_vertices = (glm::vec3*)vertex_coords;
			for (size_t j = 0; j < 3; j++)
			{
				vertices[(3 * i) + j] = tri_vertices[j];
			}
		}

		// Merge similar vertices

		size_t duplicate_vertices = 0;
		for (size_t i = 0; i < vertices.size() - duplicate_vertices; i++)
		{
			for (int j = i + 1; j < vertices.size() - duplicate_vertices; j++)
			{
				if (glm::all(glm::epsilonEqual(vertices[i], vertices[j], 0.0001f)))
				{
					std::swap(vertices[j], vertices[vertices.size() - 1 - duplicate_vertices]);
					duplicate_vertices++;
					--j;
				}
			}
		}

		vertices.erase(vertices.end() - duplicate_vertices, vertices.end());
		return vertices;
	}

	uint32_t LoadPointHull(fs::path const& _path)
	{
		if (!fs::exists(_path))
			return 0;

		if (!fs::exists(_path))
			return 0;

		std::fstream in_file(_path);
		if (!in_file.is_open())
			return 0;

		std::string name = _path.string();

		auto vertices = load_point_hull_vertices(_path);

		point_hull new_hull;
		new_hull.m_points.insert(new_hull.m_points.begin(), vertices.begin(), vertices.end());

		return Singleton<PointHullManager>().RegisterPointHull(std::move(new_hull));
	}

	void UnloadPointHull(uint32_t _handle)
	{
		Singleton<PointHullManager>().DeletePointHull(_handle);
	}


	point_hull const* PointHullManager::GetPointHull(uint32_t _handle) const
	{
		auto it = m_point_hulls.find(_handle);
		return it == m_point_hulls.end()
			? nullptr
			: it->second.get();
	}

	uint32_t PointHullManager::RegisterPointHull(point_hull&& _hull)
	{
		uint32_t const new_handle = m_handle_counter++;
		m_point_hulls.emplace(new_handle, std::make_unique<point_hull>(std::move(_hull)));
		return new_handle;
	}

	void PointHullManager::DeletePointHull(uint32_t _handle)
	{
		m_point_hulls.erase(_handle);
	}

}
}