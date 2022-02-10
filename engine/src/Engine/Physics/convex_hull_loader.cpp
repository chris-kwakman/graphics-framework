#include "convex_hull_loader.h"
#include "convex_hull.h"

#include <fstream>
#include <string>

#include <glm/gtc/epsilon.hpp>
#include <engine/Utils/singleton.h>

namespace Engine {
namespace Physics {

	uint32_t LoadConvexHull(fs::path const& _path)
	{
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
		if (!fs::exists(_path))
			return 0;

		std::vector<glm::vec3> vertices;
		std::vector<glm::uvec3> face_vertex_indices;

		std::string file_line;

		std::string name = _path.string();

		bool present_v = false, present_vn = false, present_vt = false;

		std::ifstream in_file(_path);
		char buffer[128];
		while (!in_file.eof())
		{
			std::getline(in_file, file_line, '\n');
			if (file_line[0] == '#')
				continue;
			else if (file_line.starts_with("o "))
			{
				sscanf(file_line.c_str(), "o %s", buffer);
				name = buffer;
			}
			else if (file_line.starts_with("v "))
			{
				present_v = true;
				glm::vec3 vertex;
				sscanf(file_line.c_str(), "v %f %f %f", &vertex.x, &vertex.y, &vertex.z);
				vertices.push_back(vertex);
			}
			else if (file_line.starts_with("vn "))
			{
				present_vn = true;
			}
			else if (file_line.starts_with("vt "))
			{
				present_vt = true;
			}
			else if (file_line.starts_with("f "))
			{
				glm::uvec3 indices;
				unsigned int tmp;

				const char* format;
				if (present_v && !present_vt && !present_vn)
				{
					format = "f %u %u %u";
					sscanf(file_line.c_str(), format, 
						&indices.x, &indices.y, &indices.z
					);
				}
				if (present_v && !present_vt && present_vn)
				{
					format = "f %u//%u %u//%u %u//%u";
					sscanf(file_line.c_str(), format,
						&indices.x, &tmp, &indices.y, &tmp, &indices.z, &tmp
					);
				}
				else if (present_v && present_vt && !present_vn)
				{
					format = "f %u/%u/ %u/%u/ %u/%u/";
					sscanf(file_line.c_str(), format,
						&indices.x, &tmp, &indices.y, &tmp, &indices.z, &tmp
					);
				}
				else if (present_v && present_vt && present_vn)
				{
					format = "f %u/%u/%u %u/%u/%u, %u/%u/%u";
					sscanf(file_line.c_str(), format,
						&indices.x, &tmp, &tmp,
						&indices.y, &tmp, &tmp,
						&indices.z, &tmp, &tmp
					);
				}

				indices -= 1; // OBJ indices start at 1 like absolute heathens
				face_vertex_indices.push_back(indices);
			}
		}

		convex_hull new_hull = construct_convex_hull(
			&vertices.front(), vertices.size(),
			&face_vertex_indices.front(), face_vertex_indices.size()
		);

		return Singleton<ConvexHullManager>().RegisterConvexHull(std::move(new_hull), name);
	}

	uint32_t LoadConvexHull_CS350(fs::path const& _path)
	{
		if (!fs::exists(_path))
			return 0;

		std::fstream in_file(_path);
		if (!in_file.is_open())
			return 0;

		std::string name = _path.string();

		size_t tri_count = 0;
		float vertex_coords[9];

		in_file >> tri_count;

		if (tri_count % 3 != 0)
			return 0;

		std::vector<glm::vec3> vertices(tri_count * 3);

		for (size_t i = 0; i < tri_count; i++)
		{
			for (size_t j = 0; j < 9; j++)
			{
				in_file >> vertex_coords[j];
			}
			glm::vec3 const * tri_vertices = (glm::vec3*)vertex_coords;
			for (size_t j = 0; j < 3; j++)
			{
				vertices[(3*i)+j] = tri_vertices[j];
			}
		}
	
		// Merge similar vertices

		size_t duplicate_vertices = 0;
		for (size_t i = 0; i < vertices.size() - duplicate_vertices; i++)
		{
			for (size_t j = i+1; j < vertices.size() - duplicate_vertices; j++)
			{
				if (glm::all(glm::epsilonEqual(vertices[i], vertices[j], 0.0001f)))
				{
					std::swap(vertices[j], vertices.back());
					duplicate_vertices++;
				}
			}
		}


		convex_hull new_hull = Engine::Physics::construct_convex_hull(&vertices.front(), vertices.size() - duplicate_vertices);

		return Singleton<ConvexHullManager>().RegisterConvexHull(std::move(new_hull), name);
	}

	void UnloadConvexHull(uint32_t _handle)
	{
		Singleton<ConvexHullManager>().DeleteConvexHull(_handle);
	}

}
}