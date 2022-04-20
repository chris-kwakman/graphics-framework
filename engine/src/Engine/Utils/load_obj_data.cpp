#include "load_obj_data.hpp"
#include <string>
#include <fstream>

namespace Engine {
namespace Utils {

	void Engine::Utils::load_obj_data(
		fs::path const & _path,
		std::vector<glm::uvec3>* _p_face_vertex_indices,
		std::vector<glm::vec3>* _p_vertices, 
		std::vector<glm::vec2>* _p_uvs, 
		std::vector<glm::vec3>* _p_normals
	)
	{
		std::vector<glm::uvec3> vec_vertex_indices;
		std::vector<glm::uvec3> vec_normal_indices;
		std::vector<glm::uvec3> vec_uv_indices;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;

		std::string file_line;

		std::string name = _path.string();

		bool present_v = false, present_vn = false, present_vt = false;

		if (!fs::exists(_path))
		{
			throw std::runtime_error("[convex_hull_loader] File not found.");
		}

		std::ifstream in_file(_path);
		char buffer[128];
		bool reading_vertex_data = true;
		const char* format;
		while (!in_file.eof())
		{
			std::getline(in_file, file_line, '\n');
			if (reading_vertex_data)
			{
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
					glm::vec3 normal;
					sscanf(file_line.c_str(), "vn %f %f %f", &normal.x, &normal.y, &normal.z);
					normals.push_back(normal);
				}
				else if (file_line.starts_with("vt "))
				{
					present_vt = true;
					glm::vec2 uv;
					sscanf(file_line.c_str(), "vt %f %f", &uv.x, &uv.y);
					uvs.push_back(uv);
				}
				if (file_line.starts_with("f "))
				{
					reading_vertex_data = false;
					goto reference_vertex_data;
				}
			}
			else if (file_line.starts_with("f "))
			{
			reference_vertex_data:

				glm::uvec3 vtx_indices;
				glm::uvec3 normal_indices;
				glm::uvec3 uv_indices;

				unsigned int tmp;

				const char* format;
				if (present_v && !present_vt && !present_vn)
				{
					format = "f %u %u %u";
					sscanf(file_line.c_str(), format,
						&vtx_indices.x, &vtx_indices.y, &vtx_indices.z
					);
				}
				if (present_v && !present_vt && present_vn)
				{
					format = "f %u//%u %u//%u %u//%u";
					sscanf(file_line.c_str(), format,
						&vtx_indices.x, &normal_indices.x, 
						&vtx_indices.y, &normal_indices.y, 
						&vtx_indices.z, &normal_indices.z
					);
				}
				else if (present_v && present_vt && !present_vn)
				{
					format = "f %u/%u/ %u/%u/ %u/%u/";
					sscanf(file_line.c_str(), format,
						&vtx_indices.x, &uv_indices.x, 
						&vtx_indices.y, &uv_indices.y, 
						&vtx_indices.z, &uv_indices.z
					);
				}
				else if (present_v && present_vt && present_vn)
				{
					format = "f %u/%u/%u %u/%u/%u %u/%u/%u";
					sscanf(file_line.c_str(), format,
						&vtx_indices.x, &uv_indices.x, &normal_indices.x,
						&vtx_indices.y, &uv_indices.y, &normal_indices.y,
						&vtx_indices.z, &uv_indices.z, &normal_indices.z
					);
				}

				// OBJ vtx_indices start at 1 like absolute heathens
				vec_vertex_indices.emplace_back(vtx_indices - glm::uvec3(1));
				vec_normal_indices.emplace_back(normal_indices - glm::uvec3(1));
				vec_uv_indices.emplace_back(uv_indices - glm::uvec3(1));
			}
		}

		if (_p_face_vertex_indices)
			*_p_face_vertex_indices = vec_vertex_indices;
		if (_p_vertices && present_v)
		{
			if (!_p_face_vertex_indices)
			{
				_p_vertices->reserve(vec_vertex_indices.size() * 3);
				for (auto& idx : vec_vertex_indices)
					for (size_t i = 0; i < 3; i++)
						_p_vertices->emplace_back(vertices[idx[i]]);
			}
			else
				*_p_vertices = vertices;
		}
		if (_p_uvs && present_vt)
		{
			assert(!_p_face_vertex_indices);
			_p_uvs->reserve(vec_uv_indices.size() * 3);
			for (auto& idx : vec_uv_indices)
				for (size_t i = 0; i < 3; i++)
					_p_uvs->emplace_back(uvs[idx[i]]);
		}
		if (_p_normals && present_vn)
		{
			assert(!_p_face_vertex_indices); 
			_p_normals->reserve(vec_normal_indices.size() * 3);
			for (auto& idx : vec_normal_indices)
				for (size_t i = 0; i < 3; i++)
					_p_normals->emplace_back(normals[idx[i]]);
		}

	}

}
}