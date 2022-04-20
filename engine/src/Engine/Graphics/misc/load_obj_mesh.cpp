#include "load_obj_mesh.hpp"
#include <Engine/Utils/load_obj_data.hpp>
#include <Engine/Utils/singleton.h>

#include <Engine/Physics/inertia.hpp>

namespace Engine {
namespace Graphics {

	uint32_t load_obj(fs::path const & _filepath)
	{
		auto& rm = Singleton<ResourceManager>();

		std::vector<glm::vec3> vertices, normals;
		std::vector<glm::vec2> uvs;

		std::vector<glm::uvec3> triangle_indices;

		Engine::Utils::load_obj_data(_filepath, nullptr, &vertices, &uvs , &normals);

		// Move mesh to center of mass
		if constexpr (true)
		{
			std::vector<std::array<glm::vec3, 3>> triangle_vertices(vertices.size() / 3);
			for (size_t i = 0; i < vertices.size(); i++)
				triangle_vertices[i / 3][i % 3] = vertices[i];
			float mass;
			glm::vec3 cm;
			Engine::Physics::inertiaTensorTriangles(
				triangle_vertices, &mass, &cm
			);
			for (auto& vtx : vertices)
				vtx -= cm;
		}

		unsigned int const attr_count = !vertices.empty() + !uvs.empty() + !normals.empty();

		ResourceManager::mesh_primitive_data primitive;

		std::vector<GLuint> new_gl_buffer_arr(attr_count);
		GLuint mesh_vao = 0;
		glCreateVertexArrays(1, &mesh_vao);
		glCreateBuffers(new_gl_buffer_arr.size(), &new_gl_buffer_arr.front());

		glBindVertexArray(mesh_vao);

		glBindBuffer(GL_ARRAY_BUFFER, new_gl_buffer_arr[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
		glEnableVertexArrayAttrib(mesh_vao, 0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			false,
			0,
			(GLvoid*)(0)
		);

		if (attr_count > 1 && !normals.empty())
		{
			glBindBuffer(GL_ARRAY_BUFFER, new_gl_buffer_arr[1]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * normals.size(), normals.data(), GL_STATIC_DRAW);
			glEnableVertexArrayAttrib(mesh_vao, 1);
			glVertexAttribPointer(
				1,
				3,
				GL_FLOAT,
				true,
				0,
				(GLvoid*)(0)
			);
		}

		if (attr_count > 2 && !uvs.empty())
		{
			glBindBuffer(GL_ARRAY_BUFFER, new_gl_buffer_arr[2]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * uvs.size(), uvs.data(), GL_STATIC_DRAW);
			glEnableVertexArrayAttrib(mesh_vao, 2);
			glVertexAttribPointer(
				2,
				2,
				GL_FLOAT,
				false,
				0,
				(GLvoid*)(0)
			);
		}

		glBindVertexArray(0);


		// Register previous buffers into manager.
		ResourceManager::buffer_info buffer_attr_vertices, buffer_attr_normals, buffer_attr_uvs;
		if (attr_count > 0)
		{
			buffer_attr_vertices.m_gl_id = new_gl_buffer_arr[0];
			buffer_attr_vertices.m_target = GL_ARRAY_BUFFER;
			buffer_handle const buffer_handle_vertices = rm.RegisterBuffer(buffer_attr_vertices);
		}
		if (attr_count > 1)
		{
			buffer_attr_normals.m_gl_id = new_gl_buffer_arr[1];
			buffer_attr_normals.m_target = GL_ARRAY_BUFFER;
			buffer_handle const buffer_handle_normals = rm.RegisterBuffer(buffer_attr_normals);
		}
		if (attr_count > 2)
		{
			buffer_attr_uvs.m_gl_id = new_gl_buffer_arr[2];
			buffer_attr_uvs.m_target = GL_ARRAY_BUFFER;
			buffer_handle const buffer_handle_uvs = rm.RegisterBuffer(buffer_attr_uvs);
		}

		primitive.m_index_byte_offset = 0;
		primitive.m_vertex_count = vertices.size();
		primitive.m_render_mode = GL_TRIANGLES;
		primitive.m_index_buffer_handle = 0;
		primitive.m_vao_gl_id = mesh_vao;
		primitive.m_material_handle = 0;

		ResourceManager::pbr_metallic_roughness_data pbr;
		pbr.m_base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		pbr.m_texture_base_color = 0;
		pbr.m_texture_metallic_roughness = 0;
		ResourceManager::material_data material;
		material.m_alpha_mode = ResourceManager::material_data::alpha_mode::eOPAQUE;
		material.m_double_sided = false;
		material.m_pbr_metallic_roughness = pbr;
		material.m_texture_emissive = 0;
		material.m_texture_normal = 0;
		material.m_texture_occlusion = 0;

		material_handle const mat_handle = rm.RegisterMaterial(material);
		primitive.m_material_handle = mat_handle;

		mesh_handle const new_mesh_handle = rm.RegisterMeshPrimitives(ResourceManager::mesh_primitive_list{ primitive }, _filepath.string());

		assert(new_mesh_handle != 0);

		return new_mesh_handle;

	}

	void unload_obj_mesh(uint32_t _handle)
	{
		auto & gfx_res_mgr = Singleton<ResourceManager>();
		auto & mesh_primitives = gfx_res_mgr.GetMeshPrimitives(_handle);

		// ...
	}

}
}
