#include "lighting_pass.h"
#include <Engine/Utils/singleton.h>
#include <Engine/Math/Transform3D.h>
#include <Engine/Graphics/sdl_window.h>
#include <Engine/Components/Light.h>
#include <Engine/Components/Transform.h>
#include <Engine/Components/Camera.h>
#include <Engine/Components/Renderable.h>

#include "render_common.h"

#include <glm/common.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Sandbox {

	using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;

	void RenderPointLights(
		mesh_handle _light_mesh,
		Engine::Graphics::camera_data _camera,
		Engine::Math::transform3D _camera_transform
	)
	{
		using namespace Engine::Graphics;

		auto& resource_manager = Singleton<Engine::Graphics::ResourceManager>();

		// OpenGL setup
		glCullFace(GL_FRONT);
		glDepthFunc(GL_GREATER);
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_ONE, GL_ONE);

		auto const& primitives = resource_manager.GetMeshPrimitives(_light_mesh);
		ResourceManager::mesh_primitive_data light_primitive = primitives.front();
		glBindVertexArray(light_primitive.m_vao_gl_id);
		ResourceManager::index_buffer_info ibo_info = resource_manager.GetIndexBufferInfo(light_primitive.m_index_buffer_handle);

		auto surface = Singleton<Engine::sdl_manager>().m_surface;
		glm::uvec2 const viewport_size(surface->w, surface->h);
		glm::vec2 const viewport_sizef(viewport_size);

		resource_manager.SetBoundProgramUniform(10, viewport_size);
		resource_manager.SetBoundProgramUniform(11, _camera.m_near);
		resource_manager.SetBoundProgramUniform(12, _camera.m_far);

		glm::mat4x4 const view_matrix = _camera_transform.GetInvMatrix();
		glm::mat4x4 const perspective_matrix = _camera.get_perspective_matrix();
		glm::mat4x4 const view_perspective_matrix = perspective_matrix * view_matrix;

		// Upload inverse perspective matrix for unprojecting fragment depth in shader code.
		resource_manager.SetBoundProgramUniform(7, glm::inverse(perspective_matrix));
		// Upload world to view matrix that will be used to bring normal space vectors to view space.
		resource_manager.SetBoundProgramUniform(8, view_matrix);

		glm::mat4x4 const translation(1);
		glm::mat4x4 const scale(1);

		auto collection = Singleton<Component::PointLightManager>().GetPointLightCollection();

		for (unsigned int i = 0; i < collection.m_light_count; ++i)
		{

			glm::mat4x4 const m = glm::translate(translation, collection.m_light_pos_arr[i]) *
				glm::scale(scale, 2.0f * glm::vec3(collection.m_light_radius_arr[i]));

			glm::vec3 const light_view_pos = glm::vec3(view_matrix * glm::vec4(collection.m_light_pos_arr[i], 1));

			resource_manager.SetBoundProgramUniform(5, view_perspective_matrix * m);
			glm::mat4 const view_model_matrix = view_matrix * m;
			//resource_manager.SetBoundProgramUniform(6, glm::transpose(glm::inverse(view_model_matrix)));
			//resource_manager.SetBoundProgramUniform(9, view_model_matrix);
			resource_manager.SetBoundProgramUniform(13, light_view_pos);
			resource_manager.SetBoundProgramUniform(14, collection.m_light_radius_arr[i]);
			resource_manager.SetBoundProgramUniform(15, collection.m_light_color_arr[i]);

			glDrawElements(
				light_primitive.m_render_mode,
				(GLsizei)ibo_info.m_index_count,
				ibo_info.m_type,
				(void*)0
			);
		}

		glBindVertexArray(0);
	}

	static glm::vec4 const s_ndc_corners[8] = {
		glm::vec4(-1.0f,	-1.0f,	-1.0f,	1.0f),
		glm::vec4(-1.0f,	-1.0f,	1.0f,	1.0f),
		glm::vec4(1.0f,		-1.0f,	-1.0f,	1.0f),
		glm::vec4(1.0f,		-1.0f,	1.0f,	1.0f),
		glm::vec4(1.0f,		1.0f,	-1.0f,	1.0f),
		glm::vec4(1.0f,		1.0f,	1.0f,	1.0f),
		glm::vec4(-1.0f,	1.0f,	-1.0f,	1.0f),
		glm::vec4(-1.0f,	1.0f,	1.0f,	1.0f),
	};

	void RenderDirectionalLight(Engine::Graphics::camera_data _camera, Engine::Math::transform3D _camera_transform)
	{
		using namespace Component;
		using camera_data = Engine::Graphics::camera_data;
		using texture_handle = Engine::Graphics::ResourceManager::texture_handle;
		using mesh_handle = Engine::Graphics::ResourceManager::mesh_handle;
		using shader_program = Engine::Graphics::ResourceManager::shader_program_handle;

		auto & res_mgr = Singleton<Engine::Graphics::ResourceManager>();

		DirectionalLight const dl = Singleton<Component::DirectionalLightManager>().GetDirectionalLight();
		shader_program const dl_program = res_mgr.FindShaderProgram("dirlight_shadowmap");

		// Early exit if we could not find appropriate shader program.
		// Early exit if directional light entity / component is not valid.
		if (dl_program == 0 || !dl.IsValid() || dl.Owner() == Entity::InvalidEntity)
			return;

		Transform const dl_transform = dl.Owner().GetComponent<Transform>();

		// Camera viewspace to light viewspace matrix.
		auto dl_light_world_transform = dl_transform.ComputeWorldTransform();
		glm::mat4x4 const mat_cam_view_to_world = _camera_transform.GetMatrix();
		glm::mat4x4 const mat_cam_persp_to_view = glm::inverse(_camera.get_perspective_matrix());
		glm::mat4x4 const mat_world_to_light_view = dl_light_world_transform.GetInvMatrix();
		glm::mat4x4 const mat_cam_persp_to_world = mat_cam_view_to_world * mat_cam_persp_to_view;

		// Compute frustum corners in world-space.
		glm::vec4 frustum_world_corners[8];
		for (unsigned int i = 0; i < 8; ++i)
		{
			frustum_world_corners[i] = mat_cam_persp_to_world * s_ndc_corners[i];
			frustum_world_corners[i] /= frustum_world_corners[i].w;
		}

		std::vector<std::pair<glm::vec3, glm::vec3>>	light_partition_aabbs;
		std::vector<glm::mat4x4>						light_partition_matrices;

		// Find light view partition AABBs and world space to light projection matrices.
		for (unsigned int i = 0; i < dl.GetPartitionCount(); ++i)
		{
			glm::vec4 subfrustum_corners[8];
 			float subfrustum_near = dl.GetPartitionMinDepth(i, _camera.m_near, _camera.m_far) / _camera.m_far;
			float subfrustum_far = dl.GetPartitionMinDepth(i+1, _camera.m_near, _camera.m_far) / _camera.m_far;

			for (unsigned int j = 0; j < 8; j += 2)
			{
				subfrustum_corners[j]	= frustum_world_corners[j] + subfrustum_near * (frustum_world_corners[j+1] - frustum_world_corners[j]);
				subfrustum_corners[j+1] = frustum_world_corners[j] + subfrustum_far * (frustum_world_corners[j + 1] - frustum_world_corners[j]);
			}

			// Compute AABB corners in light view space.
			glm::vec3
				aabb_min = glm::vec3(std::numeric_limits<float>::max()),
				aabb_max = glm::vec3(std::numeric_limits<float>::min());

			for (unsigned int corner = 0; corner < 8; ++corner)
			{
				subfrustum_corners[corner] = mat_world_to_light_view * subfrustum_corners[corner];
				aabb_min = glm::min(aabb_min, glm::vec3(subfrustum_corners[corner]));
				aabb_max = glm::max(aabb_max, glm::vec3(subfrustum_corners[corner]));
			}

			// Make sure X and Y of bounding box are tight.
			// Increase size of axis with minimal size
			glm::vec3 const aabb_size = aabb_max - aabb_min;
			float const aabb_diff = glm::abs(aabb_size.y - aabb_size.x);
			float const half_diff = 0.5f * aabb_diff;
			aabb_max[(int)(aabb_size.x > aabb_size.y)] += half_diff;
			aabb_min[(int)(aabb_size.x > aabb_size.y)] -= half_diff;

			// Expand AABB by moving near distance up by an occluder distance.
			// Use min and max corners to make orthogonal projection matrix.
			glm::mat4x4 const mat_light_view_to_box_projection = glm::ortho(
				aabb_min.x, aabb_max.x,
				aabb_min.y, aabb_max.y,
				-(aabb_max.z + dl.GetOccluderDistance()), -aabb_min.z // Flip since directional light is "viewing" in negative-Z direction
			);

			// Add data to vectors
			light_partition_aabbs.emplace_back(aabb_min, aabb_max);
			light_partition_matrices.emplace_back(mat_light_view_to_box_projection * mat_world_to_light_view);
		}

		// ### Render objects onto shadow map textures.

		// Collect all entity world matrices and renderables before directional light pass.
		auto entity_to_renderable_map = Singleton<RenderableManager>().GetAllRenderables();
		std::vector<glm::mat4x4> entity_world_matrices;
		std::vector<mesh_handle> entity_renderable_meshes;

		entity_world_matrices.reserve(entity_to_renderable_map.size());
		entity_renderable_meshes.reserve(entity_to_renderable_map.size());

		for (auto pair : entity_to_renderable_map)
		{
			entity_world_matrices.push_back(pair.first.GetComponent<Transform>().ComputeWorldTransform().GetMatrix());
			entity_renderable_meshes.push_back(pair.second);
		}

		// TODO: Optimizations like sorting by renderable type, culling against partition AABB's, etc...

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(1.0f);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		res_mgr.UseProgram(dl_program);
		for (unsigned int csm_partition = 0; csm_partition < dl.GetPartitionCount(); ++csm_partition)
		{
			texture_handle const csm_partition_texture = dl.GetPartitionTexture(csm_partition);
			framebuffer_handle const csm_partition_buffer = dl.GetPartitionFrameBuffer(csm_partition);
			auto csm_partition_tex_info = res_mgr.GetTextureInfo(csm_partition_texture);

			res_mgr.BindFramebuffer(csm_partition_buffer);
			glm::uvec2 csm_part_tex_size = res_mgr.GetTextureInfo(csm_partition_texture).m_size;
			GfxCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			GfxCall(glViewport(0, 0, csm_part_tex_size.x, csm_part_tex_size.y));
			
			//TODO: Pass UV for alpha-cutoff testing

			for (unsigned int e = 0; e < entity_world_matrices.size(); ++e)
			{
				glm::mat4x4 const renderable_matrix = light_partition_matrices[csm_partition] * entity_world_matrices[e];
				res_mgr.SetBoundProgramUniform(0, renderable_matrix);

				auto const & primitives = res_mgr.GetMeshPrimitives(entity_renderable_meshes[e]);
				for (unsigned int prim = 0; prim < primitives.size(); ++prim)
				{
					auto const & primitive = primitives[prim];
					GfxCall(glBindVertexArray(primitive.m_vao_gl_id));
					if (primitive.m_index_buffer_handle != 0)
					{
						auto ibo_info = res_mgr.GetIndexBufferInfo(primitive.m_index_buffer_handle);
						glDrawElements(primitive.m_render_mode, primitive.m_index_count, ibo_info.m_type, (void*)primitive.m_index_byte_offset);
					}
					else
					{
						glDrawArrays(primitive.m_render_mode, 0, primitive.m_vertex_count);
					}
				}
			}
		}
	}
}
