#include "convex_hull_loader.h"
#include "convex_hull.h"

#include <fstream>
#include <string>

#include <engine/Utils/singleton.h>

namespace Engine {
namespace Physics {

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

	void UnloadConvexHull_OBJ(uint32_t _handle)
	{
		Singleton<ConvexHullManager>().DeleteConvexHull(_handle);
	}

}
}