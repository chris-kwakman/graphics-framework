#include "create_convex_hull_mesh.h"
#include <Engine/Physics/convex_hull.h>
#include <Engine/Utils/singleton.h>

namespace Engine {
namespace Graphics {
namespace Misc {

	using namespace Engine::Physics;

	mesh_handle create_convex_hull_mesh(convex_hull const& _ch)
	{
		// Assume input convex hulls only have convex faces (hence the name).
		// This means we can create a triangle fan list for each face.

		// Copied directly to index buffer object (IBO) for triangle rendering.
		std::vector<convex_hull::vertex_idx> triangle_vertex_indices;
		// Vertex attributes
		// Vertex positions
		std::vector<glm::vec3> const mesh_vertices = _ch.m_vertices;
		// Shaders* can use this to know which triangles belong to a given face.
		std::vector<convex_hull::face_idx> triangle_face_indices;


		for (size_t face_idx = 0; face_idx < _ch.m_faces.size(); face_idx++)
		{
			convex_hull::face const& face = _ch.m_faces[face_idx];
			for (size_t i = 0; i < face.m_vertices.size() - 2; i++)
			{
				triangle_vertex_indices.emplace_back(0);
				triangle_vertex_indices.emplace_back(i);
				triangle_vertex_indices.emplace_back(i+1);
				triangle_face_indices.emplace_back(face_idx);
			}
		}
		
		using namespace Engine::Graphics;
		using RM = ResourceManager;

		auto & rm = Singleton<RM>();

		RM::mesh_primitive_data primitive;

		std::vector<GLuint> new_gl_buffer_arr;
		new_gl_buffer_arr.resize(4);
		glCreateBuffers(&new_gl_buffer_arr.front(), new_gl_buffer_arr.size());

		GLuint mesh_vao = new_gl_buffer_arr[0];
		GLuint mesh_ibo = new_gl_buffer_arr[1];
		GLuint mesh_buffer_vertices = new_gl_buffer_arr[2];
		GLuint mesh_buffer_face_indices = new_gl_buffer_arr[3];

		glBindVertexArray(mesh_vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(convex_hull::vertex_idx) * triangle_vertex_indices.size(), &triangle_vertex_indices.front(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer_vertices);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * mesh_vertices.size(), &mesh_vertices.front(), GL_STATIC_DRAW);
		glEnableVertexArrayAttrib(mesh_vao, 0);
		glVertexAttribPointer(
			0,
			3,
			GL_FLOAT,
			false,
			0,
			(GLvoid*)(0)
		);

		glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer_face_indices);
		glBufferData(GL_ARRAY_BUFFER, sizeof(convex_hull::face_idx) * triangle_face_indices.size(), &triangle_face_indices.front(), GL_STATIC_DRAW);
		glEnableVertexArrayAttrib(mesh_vao, 1);
		glVertexAttribPointer(
			1,
			1,
			GL_UNSIGNED_SHORT,
			false,
			0,
			(GLvoid*)(0)
		);

		glBindVertexArray(0);

		RM::buffer_info buffer_ibo, buffer_attr_vertices, buffer_attr_face_indices;
		buffer_ibo.m_gl_id = mesh_ibo;
		buffer_ibo.m_target = GL_ELEMENT_ARRAY_BUFFER;

		buffer_attr_vertices.m_gl_id = mesh_buffer_vertices;
		buffer_attr_vertices.m_target = GL_ARRAY_BUFFER;

		buffer_attr_vertices.m_gl_id = mesh_buffer_face_indices;
		buffer_attr_vertices.m_target = GL_ARRAY_BUFFER;

		RM::index_buffer_info ib_info;
		ib_info.m_type = GL_UNSIGNED_SHORT;
		ib_info.m_index_count = triangle_vertex_indices.size();

		// Register previous buffers into manager.
		buffer_handle const buffer_handle_ibo = rm.RegisterIndexBuffer(buffer_ibo, ib_info);
		buffer_handle const buffer_handle_vertices = rm.RegisterBuffer(buffer_attr_vertices);
		buffer_handle const buffer_handle_face_indices = rm.RegisterBuffer(buffer_attr_face_indices);

		primitive.m_index_byte_offset = 0;
		primitive.m_index_count = ib_info.m_index_count;
		primitive.m_render_mode = GL_TRIANGLES;
		primitive.m_index_buffer_handle = buffer_handle_ibo;
		primitive.m_vao_gl_id = mesh_vao;
		primitive.m_material_handle = 0;

		mesh_handle const new_mesh_handle = rm.RegisterMeshPrimitives(RM::mesh_primitive_list{ primitive });

		assert(new_mesh_handle != 0);

		return new_mesh_handle;
	}

}
}
}