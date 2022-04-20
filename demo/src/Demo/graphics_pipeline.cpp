
#include "render_common.h"
#include "graphics_pipeline.h"
#include "GraphicsPipeline/lighting_pass.h"
#include "GraphicsPipeline/volumetric_fog.h"

// Engine File Includes
#include <Engine/Graphics/manager.h>
#include <Engine/Graphics/sdl_window.h>
#include <Engine/Editor/editor.h>
#include <Engine/Utils/singleton.h>
#include <Engine/ECS/entity.h>

// Components
#include <Engine/Components/Camera.h>
#include <Engine/Components/CurveInterpolator.h>
#include <Engine/Components/Light.h>
#include <Engine/Components/Renderable.h>
#include <Engine/Components/SkeletonAnimator.h>
#include <Engine/Components/Transform.h>
#include <Engine/Physics/Collider.h>
#include <Engine/Physics/convex_hull.h>
#include <Engine/Physics/physics_manager.hpp>

// GLM
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct gfx_pipeline_resources
{
	using mesh_handle = Engine::Graphics::mesh_handle;
	using buffer_handle = Engine::Graphics::buffer_handle;

	static size_t const MAX_LINE_SEGMENTS = 512;
	static size_t const MAX_POINT_COUNT = MAX_LINE_SEGMENTS * 2;
	mesh_handle line_point_mesh_handle = 0;
	buffer_handle line_point_mesh_vbo_handle = 0;
	void upload_line_point_data(glm::vec3 const* _points, size_t _count) const;
};

static gfx_pipeline_resources s_pipeline_resources;

namespace Sandbox
{
	void SetupGraphicsPipelineRender()
	{
		setup_lighting_pass_pipeline();
		setup_volumetric_fog();

		using namespace Engine::Graphics;
		using GraphicsManager = ResourceManager;
		auto& gfx_mgr = Singleton<GraphicsManager>();

		// Create mesh for displaying penetration_points / lines.
		GLuint line_point_mesh_vao = 0, line_point_mesh_vbo = 0;
		glCreateVertexArrays(1, &line_point_mesh_vao);
		glCreateBuffers(1, &line_point_mesh_vbo);

		glBindVertexArray(line_point_mesh_vao);
		glBindBuffer(GL_ARRAY_BUFFER, line_point_mesh_vbo);
		glBufferData(GL_ARRAY_BUFFER, gfx_pipeline_resources::MAX_POINT_COUNT * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexArrayAttrib(line_point_mesh_vao, 0);
		glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);

		GraphicsManager::buffer_info line_point_buf_info;
		line_point_buf_info.m_gl_id = line_point_mesh_vbo;
		line_point_buf_info.m_target = GL_ARRAY_BUFFER;
		s_pipeline_resources.line_point_mesh_vbo_handle = gfx_mgr.RegisterBuffer(line_point_buf_info);

		GraphicsManager::material_data line_point_mesh_mat;
		line_point_mesh_mat.m_double_sided = false;
		line_point_mesh_mat.m_name = "Debug Line Point Mesh Material";
		line_point_mesh_mat.m_pbr_metallic_roughness.m_base_color_factor = glm::vec4(1.0f);
		material_handle const line_point_mat_handle = gfx_mgr.RegisterMaterial(line_point_mesh_mat);

		GraphicsManager::mesh_primitive_data prim_data;
		prim_data.m_vao_gl_id = line_point_mesh_vao;
		prim_data.m_vertex_count = gfx_pipeline_resources::MAX_POINT_COUNT;
		prim_data.m_material_handle = line_point_mat_handle;

		s_pipeline_resources.line_point_mesh_handle = gfx_mgr.RegisterMeshPrimitives(
			GraphicsManager::mesh_primitive_list{ prim_data }, 
			"Debug Line Point Mesh"
		);
	}
	void ShutdownGraphicsPipelineRender()
	{
		shutdown_lighting_pass_pipeline();
		shutdown_volumetric_fog();

		s_pipeline_resources.line_point_mesh_handle = 0;
		s_pipeline_resources.line_point_mesh_vbo_handle = 0;
	}

	void GraphicsPipelineRender()
	{
		///////////////////////////////////////////
		//		Scene Rendering Pipeline
		///////////////////////////////////////////

		using namespace Engine;
		using namespace Engine::ECS;

		Graphics::ResourceManager & res_mgr = Singleton<Graphics::ResourceManager>();

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		GfxCall(glViewport(0, 0, Singleton<Engine::sdl_manager>().m_surface->w, Singleton<Engine::sdl_manager>().m_surface->h));

		using index_buffer_handle = Engine::Graphics::buffer_handle;
		using namespace Engine::Graphics;

		auto cameras = Singleton<Component::CameraManager>().AllCameras();
		Entity camera_entity = Entity::InvalidEntity; 
		Entity const editor_cam_entity = Singleton<Engine::Editor::Editor>().EditorCameraEntity;
		if (cameras.size() > 1)
		{
			for (auto const & pair : cameras)
			{
				if (pair.first != editor_cam_entity)
				{
					camera_entity = pair.first;
				}
			}
		}
		else
			camera_entity = editor_cam_entity;

		auto const cam_transform = camera_entity.GetComponent<Component::Transform>().ComputeWorldTransform();
		glm::vec3 const camera_forward = cam_transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 const camera_right = glm::normalize(glm::cross(camera_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 const camera_up = glm::normalize(glm::cross(camera_forward, camera_right));

		//s_camera_transform.quaternion = camera.get_lookat_quat();

		using shader_program_handle = Engine::Graphics::shader_program_handle;

		shader_program_handle const program_draw_gbuffer = res_mgr.FindShaderProgram("draw_gbuffer");
		shader_program_handle const program_draw_gbuffer_skinned = res_mgr.FindShaderProgram("draw_gbuffer_skinned");
		shader_program_handle const program_draw_framebuffer_plain = res_mgr.FindShaderProgram("draw_framebuffer_plain");
		shader_program_handle const program_draw_global_light = res_mgr.FindShaderProgram("draw_framebuffer_global_light");
		assert(program_draw_gbuffer != 0);

		auto camera_component = camera_entity.GetComponent<Component::Camera>();
		glm::mat4 const camera_view_matrix = cam_transform.GetInvMatrix();
		auto camera_data = camera_component.GetCameraData();

		glm::mat4 const camera_perspective_matrix = camera_data.is_orthogonal_camera() ?
			camera_data.get_orthogonal_matrix() : camera_data.get_perspective_matrix();
		glm::mat4 const matrix_vp = camera_perspective_matrix * camera_view_matrix;

		ubo_camera_data new_cam_data_ubo(cam_transform, camera_data);
		new_cam_data_ubo.m_viewport_size = Singleton<sdl_manager>().get_window_size();
		new_cam_data_ubo.m_inv_vp = glm::inverse(matrix_vp);
		new_cam_data_ubo.m_v = camera_view_matrix;
		new_cam_data_ubo.m_p = camera_perspective_matrix;
		new_cam_data_ubo.m_near = camera_data.m_near;
		new_cam_data_ubo.m_far = camera_data.m_far;
		new_cam_data_ubo.m_view_dir = cam_transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		update_camera_ubo(new_cam_data_ubo);

		auto all_renderables = Singleton<Component::RenderableManager>().GetAllRenderables();

		std::vector<decltype(all_renderables)::value_type> sorted_renderables;
		// Sort skin-less renderables from skinned renderables.
		std::copy_if(
			all_renderables.begin(), all_renderables.end(), std::back_inserter(sorted_renderables),
			[](decltype(all_renderables)::value_type const& pair)->bool
			{
				return !pair.first.HasComponent<Component::Skin>();
			}
		);
		unsigned int first_skinned_renderable_index = (unsigned int)sorted_renderables.size();
		std::copy_if(
			all_renderables.begin(), all_renderables.end(), std::back_inserter(sorted_renderables),
			[](decltype(all_renderables)::value_type const& pair)->bool
			{
				return pair.first.HasComponent<Component::Skin>();
			}
		);


		res_mgr.BindFramebuffer(s_framebuffer_gbuffer);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		auto window_size = Singleton<Engine::sdl_manager>().get_window_size();
		glViewport(0, 0, window_size.x, window_size.y);

		int LOC_SAMPLER_DEPTH = -1;
		int LOC_SAMPLER_BASE_COLOR = -1;
		int LOC_SAMPLER_METALLIC = -1;
		int LOC_SAMPLER_NORMAL = -1;
		int LOC_BASE_COLOR_FACTOR = -1;
		int LOC_ALPHA_CUTOFF = -1;
		int LOC_MAT_MVP = -1;
		int LOC_MAT_MVP_INV = -1;
		int LOC_MAT_MV = -1;
		int LOC_MAT_V = -1;
		int LOC_MAT_VP = -1;
		int LOC_MAT_P = -1;
		int LOC_MAT_P_INV = -1;
		int LOC_MAT_MV_T_INV = -1;
		int LOC_MAT_JOINT_0 = -1;

		auto set_bound_program_uniform_locations = [&]()
		{
			LOC_SAMPLER_DEPTH = res_mgr.FindBoundProgramUniformLocation("u_sampler_depth");
			LOC_SAMPLER_BASE_COLOR = res_mgr.FindBoundProgramUniformLocation("u_sampler_base_color");
			LOC_SAMPLER_METALLIC = res_mgr.FindBoundProgramUniformLocation("u_sampler_metallic_roughness");
			LOC_SAMPLER_NORMAL = res_mgr.FindBoundProgramUniformLocation("u_sampler_normal");
			LOC_BASE_COLOR_FACTOR = res_mgr.FindBoundProgramUniformLocation("u_base_color_factor");
			LOC_ALPHA_CUTOFF = res_mgr.FindBoundProgramUniformLocation("u_alpha_cutoff");

			LOC_MAT_MVP = res_mgr.FindBoundProgramUniformLocation("u_mvp");
			LOC_MAT_MVP_INV = res_mgr.FindBoundProgramUniformLocation("u_mvp_inv");
			LOC_MAT_MV = res_mgr.FindBoundProgramUniformLocation("u_mv");
			LOC_MAT_V = res_mgr.FindBoundProgramUniformLocation("u_v");
			LOC_MAT_P = res_mgr.FindBoundProgramUniformLocation("u_p");
			LOC_MAT_VP = res_mgr.FindBoundProgramUniformLocation("u_vp");
			LOC_MAT_P_INV = res_mgr.FindBoundProgramUniformLocation("u_p_inv");
			LOC_MAT_MV_T_INV = res_mgr.FindBoundProgramUniformLocation("u_mv_t_inv");
			LOC_MAT_JOINT_0 = res_mgr.FindBoundProgramUniformLocation("u_joint_matrices[0]");
		};

		//	#
		//	#	Deferred Rendering
		//	#

		//
		//	Renderable Rendering
		//

		res_mgr.UseProgram(program_draw_gbuffer);
		set_bound_program_uniform_locations();

		for (unsigned int renderable_idx = 0; renderable_idx < sorted_renderables.size(); ++renderable_idx)
		{
			Engine::ECS::Entity const renderable_entity = sorted_renderables[renderable_idx].first;
			mesh_handle const renderable_mesh = sorted_renderables[renderable_idx].second;
			if (renderable_mesh == 0)
				continue;

			// Use skinned renderable program once we reach skinned portion of renderable array.
			if (renderable_idx == first_skinned_renderable_index)
			{
				res_mgr.UseProgram(program_draw_gbuffer_skinned);
				set_bound_program_uniform_locations();
			}

			Component::Transform renderable_transform = renderable_entity.GetComponent<Component::Transform>();
			// TODO: Use cached world matrix in transform manager (once implemented)
			auto model_transform = renderable_transform.ComputeWorldTransform();
			glm::mat4 const mesh_model_matrix = model_transform.GetMatrix();
			glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
			glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

			res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp * mesh_model_matrix);
			res_mgr.SetBoundProgramUniform(LOC_MAT_MV, matrix_mv);
			res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, matrix_t_inv_mv);

			if (renderable_idx >= first_skinned_renderable_index)
			{
				Component::Skin const skin_component = renderable_entity.GetComponent<Component::Skin>();
				std::vector<Component::Transform> const& skin_skeleton_instance_nodes = skin_component.GetSkeletonInstanceNodes();
				std::vector<glm::mat4x4> const& skin_inv_bind_matrices = skin_component.GetSkinNodeInverseBindMatrices();
				std::vector<glm::mat4x4> joint_skinning_matrices;
				Engine::Math::transform3D const skeleton_root_inv_transform =
					skin_component.GetSkeletonRootNode().GetParent().GetComponent<Component::Transform>().ComputeWorldTransform().GetInverse();
				assert(skin_skeleton_instance_nodes.size() <= 128);
				joint_skinning_matrices.reserve(skin_skeleton_instance_nodes.size());
				// Compute joint->model matrices
				for (Component::Transform const& joint_node : skin_skeleton_instance_nodes)
				{
					// TODO: Use cached world matrix in transform manager (once implemented)
					joint_skinning_matrices.emplace_back(
						(skeleton_root_inv_transform * joint_node.ComputeWorldTransform()).GetMatrix()
					);
				}
				// Compute final skinning matrix for each joint.
				for (unsigned int j = 0; j < skin_inv_bind_matrices.size(); ++j)
					joint_skinning_matrices[j] = joint_skinning_matrices[j] * skin_inv_bind_matrices[j];

				for (unsigned int j = 0; j < joint_skinning_matrices.size(); ++j)
					res_mgr.SetBoundProgramUniform(LOC_MAT_JOINT_0 + j, joint_skinning_matrices[j]);
			}

			auto& mesh_primitives = res_mgr.GetMeshPrimitives(renderable_mesh);
			for (unsigned int primitive_idx = 0; primitive_idx < mesh_primitives.size(); ++primitive_idx)
			{
				ResourceManager::mesh_primitive_data primitive = mesh_primitives[primitive_idx];

				// Set texture slots
				if (primitive.m_material_handle != 0)
				{
					ResourceManager::material_data material = res_mgr.GetMaterial(primitive.m_material_handle);

					texture_handle const use_base_color = material.m_pbr_metallic_roughness.m_texture_base_color
						? material.m_pbr_metallic_roughness.m_texture_base_color
						: s_texture_white;
					texture_handle const use_metallic_roughness = material.m_pbr_metallic_roughness.m_texture_metallic_roughness
						? material.m_pbr_metallic_roughness.m_texture_metallic_roughness
						: s_texture_white;

					// TODO: Implement metallic roughness color factors
					activate_texture(use_base_color, LOC_SAMPLER_BASE_COLOR, 0);
					activate_texture(use_metallic_roughness, LOC_SAMPLER_METALLIC, 1);
					activate_texture(material.m_texture_normal, LOC_SAMPLER_NORMAL, 2);
					res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, material.m_pbr_metallic_roughness.m_base_color_factor);
					/*activate_texture(material.m_texture_occlusion, 3, 3);
					activate_texture(material.m_texture_emissive, 4, 4);*/


					using alpha_mode = ResourceManager::material_data::alpha_mode;
					res_mgr.SetBoundProgramUniform(
						LOC_ALPHA_CUTOFF,
						(float)(material.m_alpha_mode == alpha_mode::eMASK ? material.m_alpha_cutoff : 0.0)
					);
					if (material.m_alpha_mode == alpha_mode::eBLEND)
						glEnable(GL_BLEND);
					else
						glDisable(GL_BLEND);

					if (material.m_double_sided)
						glDisable(GL_CULL_FACE);
					else
						glEnable(GL_CULL_FACE);
				}

				render_primitive(primitive);
			}

		}

		//
		//	Decal Rendering
		//

		// TODO: Only apply decal against static objects (not dynamic ones).
		// No concept for static objects exists at the moment of writing this, but this might be
		// important for future ideas (i.e. physics & collisions course).

		res_mgr.BindFramebuffer(s_framebuffer_gbuffer_decal);

		auto const& all_decals = Singleton<Component::DecalManager>().GetAllDecals();
		if (!all_decals.empty() && Singleton<Component::DecalManager>().s_render_decals)
		{
			mesh_handle const cube_mesh = res_mgr.FindMesh("Cube/cube");
			auto const& cube_mesh_primitives = res_mgr.GetMeshPrimitives(cube_mesh);

			using E_DecalRenderMode = Component::DecalManager::E_DecalRenderMode;
			E_DecalRenderMode const render_mode = Singleton<Component::DecalManager>().GetDecalRenderMode();

			res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_gbuffer_decals"));
			set_bound_program_uniform_locations();

			// Forcefully rotate this cube mesh because for some reason mesh provided by corresponding GLTF does not take
			// into account the rotation of the owning model when it is loaded in.
			glm::quat const cube_rot = glm::quatLookAt(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
			glm::mat4x4 const mat_cube_rot = glm::toMat4(cube_rot);

			res_mgr.SetBoundProgramUniform(LOC_ALPHA_CUTOFF, 0.5f);
			res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(1.0f));
			res_mgr.SetBoundProgramUniform(LOC_MAT_VP, matrix_vp);
			res_mgr.SetBoundProgramUniform(LOC_MAT_P_INV, glm::inverse(camera_perspective_matrix));

			// TODO: Use subroutine instead of uniform set every frame.
			int LOC_DECAL_RENDER_MODE = res_mgr.GetBoundProgramUniformLocation("u_decal_render_mode");
			int LOC_VIEWPORT_SIZE = res_mgr.GetBoundProgramUniformLocation("u_viewport_size");
			res_mgr.SetBoundProgramUniform(LOC_DECAL_RENDER_MODE, (int)render_mode);
			res_mgr.SetBoundProgramUniform(LOC_VIEWPORT_SIZE, Singleton<Engine::sdl_manager>().get_window_size());

			glDepthFunc(GL_LEQUAL);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_SRC_ALPHA);
			glEnable(GL_CULL_FACE);
			if (render_mode == E_DecalRenderMode::eDecalBoundingVolume)
			{
				glDepthMask(GL_TRUE);

				activate_texture(s_texture_white, LOC_SAMPLER_BASE_COLOR, 0);
				activate_texture(s_texture_white, LOC_SAMPLER_NORMAL, 1);
				activate_texture(s_texture_white, LOC_SAMPLER_METALLIC, 2);

				for (auto decal_pair : all_decals)
				{
					Entity decal_entity = decal_pair.first;
					Component::Transform decal_transform = decal_entity.GetComponent<Component::Transform>();
					glm::mat4x4 const world_matrix = decal_transform.ComputeWorldMatrix() * mat_cube_rot;
					glm::mat4x4 const mv = camera_view_matrix * world_matrix;
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, glm::transpose(glm::inverse(mv)));
					res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, camera_perspective_matrix * mv);

					for (mesh_primitive_data const& prim : cube_mesh_primitives)
					{
						render_primitive(prim);
					}
				}
			}
			else
			{
				glDepthMask(GL_FALSE);
				glCullFace(GL_FRONT);
				glDepthFunc(GL_GEQUAL);

				int LOC_DECAL_ANGLE_TRESHHOLD = res_mgr.FindBoundProgramUniformLocation("u_decal_angle_treshhold");
				int LOC_SAMPLER_DEFERRED_DEPTH = res_mgr.FindBoundProgramUniformLocation("u_sampler_deferred_depth");
				int LOC_SAMPLER_DEFERRED_NORMAL = res_mgr.FindBoundProgramUniformLocation("u_sampler_deferred_normal");
				activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEFERRED_DEPTH, 3);
				activate_texture(s_fb_texture_normal, LOC_SAMPLER_DEFERRED_NORMAL, 4);
				float clamped_angle_treshhold = Singleton<Component::DecalManager>().s_decal_angle_treshhold;
				clamped_angle_treshhold = clamped_angle_treshhold < 0.0f ? 0.0f : clamped_angle_treshhold;
				clamped_angle_treshhold = clamped_angle_treshhold > 90.0f ? 90.0f : clamped_angle_treshhold;
				res_mgr.SetBoundProgramUniform(
					LOC_DECAL_ANGLE_TRESHHOLD,
					clamped_angle_treshhold * (glm::pi<float>() / 180.0f)
				);

				for (auto decal_pair : all_decals)
				{
					Component::decal_textures const& decal_textures = decal_pair.second;
					if (render_mode == E_DecalRenderMode::eDecalMask)
					{
						activate_texture(s_texture_white, LOC_SAMPLER_BASE_COLOR, 0);
						activate_texture(s_texture_white, LOC_SAMPLER_NORMAL, 1);
						activate_texture(s_texture_white, LOC_SAMPLER_METALLIC, 2);
					}
					else
					{
						activate_texture(decal_textures.m_texture_albedo, LOC_SAMPLER_BASE_COLOR, 0);
						activate_texture(decal_textures.m_texture_normal, LOC_SAMPLER_NORMAL, 1);
						activate_texture(decal_textures.m_texture_metallic_roughness, LOC_SAMPLER_METALLIC, 2);
					}

					Entity decal_entity = decal_pair.first;
					Component::Transform decal_transform = decal_entity.GetComponent<Component::Transform>();
					glm::mat4x4 const world_matrix = decal_transform.ComputeWorldMatrix();
					glm::mat4x4 const mv = camera_view_matrix * world_matrix;
					glm::mat4x4 const mvp = camera_perspective_matrix * mv;
					glm::mat4x4 const mvp_inv = glm::inverse(mvp);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, glm::transpose(glm::inverse(mv)));
					res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, mvp);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MVP_INV, mvp_inv);

					for (mesh_primitive_data const& prim : cube_mesh_primitives)
					{
						render_primitive(prim);
					}
				}
			}


		}

		//
		//	Ambient Occlusion
		//

		res_mgr.BindFramebuffer(s_framebuffer_ao);

		// Set viewport size to size of final AO texture.
		auto ao_texture_info = res_mgr.GetTextureInfo(s_fb_texture_ao);
		glViewport(0, 0, ao_texture_info.m_size.x, ao_texture_info.m_size.y);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		if (!s_ambient_occlusion.disable)
		{

			glDepthMask(GL_TRUE);
			glDepthFunc(GL_ALWAYS);
			glDisable(GL_BLEND);

			shader_program_handle const ao_program = res_mgr.FindShaderProgram("draw_framebuffer_ambient_occlusion");
			res_mgr.UseProgram(ao_program);
			set_bound_program_uniform_locations();

			if(LOC_MAT_P != -1)
				res_mgr.SetBoundProgramUniform(LOC_MAT_P, camera_perspective_matrix);
			if(LOC_MAT_P_INV != -1)
				res_mgr.SetBoundProgramUniform(LOC_MAT_P_INV, glm::inverse(camera_perspective_matrix));

			auto set_if_found = [&res_mgr](const char* _name, auto const& _value)
			{
				int LOC_VALUE = res_mgr.FindBoundProgramUniformLocation(_name);
				if (LOC_VALUE != -1)
					res_mgr.SetBoundProgramUniform(LOC_VALUE, _value);
			};

			auto const & s_ao = s_ambient_occlusion;
			set_if_found("u_ao.radius", s_ao.radius_scale);
			set_if_found("u_ao.angle_bias", s_ao.angle_bias);
			set_if_found("u_ao.attenuation_scale", s_ao.attenuation_scale);
			set_if_found("u_ao.ao_scale", s_ao.ao_scale);
			set_if_found("u_ao.sample_directions", s_ao.sample_directions);
			set_if_found("u_ao.sample_steps", s_ao.sample_steps);

			activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEPTH, 0);

			GfxCall(glBindVertexArray(0));
			GfxCall(glDrawArrays(GL_TRIANGLES, 0, 3));
		}

		//
		//	AO Blurring Pass
		//
		GLenum const attachments_ao_pingpong[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		res_mgr.BindFramebuffer(s_framebuffer_ao_pingpong);
		auto ao_pingpong_texture_info = res_mgr.GetTextureInfo(s_fb_texture_ao_pingpong);
		glm::uvec2 ao_pp_size = ao_pingpong_texture_info.m_size;
		glViewport(0, 0, ao_pp_size.x, ao_pp_size.y);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (!s_ambient_occlusion.disable)
		{

			// Blit ambient occlusion texture into pingpong texture.

			glBlitNamedFramebuffer(
				s_framebuffer_ao, s_framebuffer_ao_pingpong,
				0, 0, ao_texture_info.m_size.x, ao_texture_info.m_size.y,
				0, 0, ao_pp_size.x, ao_pp_size.y,
				GL_COLOR_BUFFER_BIT,
				GL_LINEAR
			);
			
			glDepthFunc(GL_ALWAYS);
			glDisable(GL_BLEND);

			res_mgr.UseProgram(res_mgr.FindShaderProgram("blur_ambient_occlusion"));
			//set_bound_program_uniform_locations();
			int LOC_SAMPLER_DEPTH = res_mgr.FindBoundProgramUniformLocation("u_sampler_depth");
			int LOC_SAMPLER_AO_INPUT = res_mgr.FindBoundProgramUniformLocation("u_sampler_ao");
			int LOC_SIGMA = res_mgr.FindBoundProgramUniformLocation("u_sigma");
			int LOC_BLUR_HORIZONTAL = res_mgr.FindBoundProgramUniformLocation("u_bool_blur_horizontal");
			int LOC_CAMERA_NEAR = res_mgr.FindBoundProgramUniformLocation("u_camera_near");
			int LOC_CAMERA_FAR = res_mgr.FindBoundProgramUniformLocation("u_camera_far");

			res_mgr.SetBoundProgramUniform(LOC_SIGMA, s_ambient_occlusion.sigma);
			res_mgr.SetBoundProgramUniform(LOC_CAMERA_NEAR, camera_component.GetNearDistance());
			res_mgr.SetBoundProgramUniform(LOC_CAMERA_FAR, camera_component.GetFarDistance());
			activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEPTH, 0);
			activate_texture(s_fb_texture_ao_pingpong, LOC_SAMPLER_AO_INPUT, 1);

			for (unsigned int i = 0; i < (unsigned int)s_ambient_occlusion.blur_passes; ++i)
			{
				res_mgr.SetBoundProgramUniform(LOC_BLUR_HORIZONTAL, (i%2) == 0);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

		}

		// 
		// Skeleton Node Rendering
		// 

		res_mgr.BindFramebuffer(s_framebuffer_gbuffer);

		// Render debug all skeleton nodes
		auto skin_entity_map = Singleton<Component::SkinManager>().GetAllSkinEntities();
		if (!skin_entity_map.empty())
		{
			res_mgr.UseProgram(program_draw_gbuffer);

			set_bound_program_uniform_locations();

			activate_texture(s_texture_white, LOC_SAMPLER_BASE_COLOR, 0);
			activate_texture(s_texture_white, LOC_SAMPLER_METALLIC, 1);
			activate_texture(s_texture_white, LOC_SAMPLER_NORMAL, 2);

			glDepthFunc(GL_LEQUAL);
			glDisable(GL_BLEND);

			for (auto skin_entity_pair : skin_entity_map)
			{
				if (!skin_entity_pair.second.m_render_joints)
					continue;

				Component::Transform skeleton_root = skin_entity_pair.second.m_skeleton_root;
				Engine::Math::transform3D const root_transform = skeleton_root.ComputeWorldTransform();
				float joint_scale = 0.05f * glm::min(root_transform.scale.x, glm::min(root_transform.scale.y, root_transform.scale.z));

				std::vector<Component::Transform> const& skeleton_nodes = skin_entity_pair.second.m_skeleton_instance_nodes;

				std::vector<Component::Transform> traverse_nodes;
				traverse_nodes.push_back(skeleton_root);
				for (unsigned int i = 0; i < traverse_nodes.size(); ++i)
				{
					Component::Transform const current_parent = traverse_nodes[i];
					Engine::Math::transform3D parent_world_transform = current_parent.ComputeWorldTransform();

					std::vector<Entity> node_children = current_parent.GetChildren();
					for (auto child : node_children)
					{
						// Skip if child node is not part of skeleton node list.
						auto iter = std::find(skeleton_nodes.begin(), skeleton_nodes.end(), child.GetComponent<Component::Transform>());
						if (iter == skeleton_nodes.end())
							continue;
						traverse_nodes.push_back(child.GetComponent<Component::Transform>());

						Engine::Math::transform3D child_world_transform = child.GetComponent<Component::Transform>().ComputeWorldTransform();

						glm::vec3 dir_vec = parent_world_transform.position - child_world_transform.position;
						Engine::Math::transform3D bone_transform;
						bone_transform.rotation = glm::quatLookAt(glm::normalize(dir_vec), glm::vec3(0.0f, 1.0f, 0.0f));
						float dir_vec_length = glm::length(dir_vec);
						bone_transform.scale = glm::vec3(0.25f, 0.25f, 1.0f) * dir_vec_length;

						child_world_transform.scale = glm::vec3(1.0f);
						child_world_transform.rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
						child_world_transform.rotation.w = 1.0f;



						//// TODO: Use cached world matrix in transform manager (once implemented)
						auto model_transform = child_world_transform * bone_transform;
						glm::mat4 const mesh_model_matrix = model_transform.GetMatrix();
						glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
						glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

						res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp * mesh_model_matrix);
						res_mgr.SetBoundProgramUniform(LOC_MAT_MV, matrix_mv);
						res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, matrix_t_inv_mv);

						// Set base color factor
						res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
						// Set alpha-cutoff value to zero
						res_mgr.SetBoundProgramUniform(LOC_ALPHA_CUTOFF, 0.0f);

						glBindVertexArray(s_gl_bone_vao);
						GfxCall(glDrawElements(
							GL_TRIANGLES,
							joint_index_count,
							GL_UNSIGNED_INT,
							0
						));
					}
				}
			}
		}


		{
			//
			// Debug render for curves and curve nodes
			//

			auto& curve_interpolator = Singleton<Component::CurveInterpolatorManager>();
			auto curve_entities = curve_interpolator.GetRenderableCurves();
			if (!curve_entities.empty())
			{
				res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_gbuffer_primitive"));

				set_bound_program_uniform_locations();

				res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, Component::COLOR_CURVE_LINE);

				// Line Rendering

				glDepthMask(GL_TRUE);
				glDepthFunc(GL_ALWAYS);
				glDisable(GL_BLEND);
				glLineWidth(10.0f);

				glBindVertexArray(s_gl_line_vao);
				for (Component::CurveInterpolator curve_comp : curve_entities)
				{
					Component::Transform renderable_transform = curve_comp.Owner().GetComponent<Component::Transform>();
					// TODO: Use cached world matrix in transform manager (once implemented)
					auto model_transform = renderable_transform.ComputeWorldTransform();
					glm::mat4 const mesh_model_matrix = model_transform.GetMatrix();
					glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
					glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

					res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp * mesh_model_matrix);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV, matrix_mv);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, matrix_t_inv_mv);

					auto curve = curve_comp.GetPiecewiseCurve();
					set_line_mesh(&curve.m_lut.m_points.front(), (unsigned int)curve.m_lut.m_points.size());
					GfxCall(glDrawElements(
						GL_LINE_STRIP,
						(GLsizei)curve.m_lut.m_points.size(),
						GL_UNSIGNED_INT,
						(void*)0
					));
				}
			}

			// Node Rendering

			auto curve_node_entities = curve_interpolator.GetRenderableCurveNodes();
			if (!curve_node_entities.empty())
			{
				res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_gbuffer_primitive"));

				set_bound_program_uniform_locations();

				mesh_handle box_mesh = res_mgr.FindMesh("Box/Mesh");
				auto const& box_primitives = res_mgr.GetMeshPrimitives(box_mesh);
				auto const& primitive = box_primitives.front();

				std::vector<glm::vec3> line_mesh;

				for (Component::CurveInterpolator curve_comp : curve_node_entities)
				{
					glBindVertexArray(primitive.m_vao_gl_id);
					res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, Component::COLOR_NODE_POSITION);

					glDepthMask(GL_TRUE);
					glDepthFunc(GL_ALWAYS);
					glDisable(GL_BLEND);

					Engine::Math::transform3D node_transform;
					node_transform.scale = glm::vec3(0.5f);

					using curve_type = Component::piecewise_curve::EType;
					Component::piecewise_curve const& curve = curve_comp.GetPiecewiseCurve();

					for (unsigned int i = 0; i < curve.m_nodes.size(); ++i)
					{
						glm::vec3 const node = curve.m_nodes[i];
						glm::vec3 node_modified = node;

						if (curve.m_type == curve_type::Bezier || curve.m_type == curve_type::Hermite)
						{
							int remainder = (i % 3);
							if (remainder == 0)
								res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, Component::COLOR_NODE_POSITION);
							else
							{
								res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, Component::COLOR_NODE_TANGENT);
								if (remainder == 1)
								{
									node_modified += curve.m_nodes[i - 1];
									// Add nodes to line mesh for rendering lines between positional nodes and tangent / intermediate nodes.
									line_mesh.emplace_back(curve.m_nodes[i - 1]);
									line_mesh.emplace_back(node_modified);
								}
								else
								{
								node_modified += curve.m_nodes[i + 1];
								// Add nodes to line mesh for rendering lines between positional nodes and tangent / intermediate nodes.
								line_mesh.emplace_back(curve.m_nodes[i + 1]);
								line_mesh.emplace_back(node_modified);
								}

							}
						}

						node_transform.position = node_modified;
						Component::Transform renderable_transform = curve_comp.Owner().GetComponent<Component::Transform>();
						// TODO: Use cached world matrix in transform manager (once implemented)
						glm::mat4 const mesh_model_matrix = renderable_transform.ComputeWorldMatrix() * node_transform.GetMatrix();
						glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
						glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

						res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp* mesh_model_matrix);
						res_mgr.SetBoundProgramUniform(LOC_MAT_MV, matrix_mv);
						res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, matrix_t_inv_mv);


						auto ibo_comp_type = res_mgr.GetIndexBufferInfo(primitive.m_index_buffer_handle).m_type;
						render_primitive(primitive);
					}
					set_line_mesh(&line_mesh[0], (unsigned int)line_mesh.size());

					Component::Transform curve_transform = curve_comp.Owner().GetComponent<Component::Transform>();
					// TODO: Use cached world matrix in transform manager (once implemented)
					auto model_transform = curve_transform.ComputeWorldTransform();
					glm::mat4 const mesh_model_matrix = model_transform.GetMatrix();
					glm::mat4 const matrix_mv = camera_view_matrix * mesh_model_matrix;
					glm::mat4 const matrix_t_inv_mv = glm::transpose(glm::inverse(matrix_mv));

					res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
					res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp* mesh_model_matrix);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV, matrix_mv);
					res_mgr.SetBoundProgramUniform(LOC_MAT_MV_T_INV, matrix_t_inv_mv);

					glDepthMask(GL_TRUE);
					glDepthFunc(GL_ALWAYS);
					glDisable(GL_BLEND);
					glLineWidth(5.0f);

					glBindVertexArray(s_gl_line_vao);
					GfxCall(glDrawElements(
						GL_LINES,
						(GLsizei)line_mesh.size(),
						GL_UNSIGNED_INT,
						(void*)0
					));

					line_mesh.clear();
				}
			}

			// LUT rendering

			auto curve_lut_entities = curve_interpolator.GetRenderableCurveLUTs();
			if (!curve_lut_entities.empty())
			{
				res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_gbuffer_primitive"));

				set_bound_program_uniform_locations();

				mesh_handle box_mesh = res_mgr.FindMesh("Box/Mesh");
				auto const& box_primitives = res_mgr.GetMeshPrimitives(box_mesh);
				auto const& primitive = box_primitives.front();

				glBindVertexArray(primitive.m_vao_gl_id);
				res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, Component::COLOR_CURVE_LUT_POINT);

				Engine::Math::transform3D node_transform;
				node_transform.scale = glm::vec3(0.25f);
				for (Component::CurveInterpolator curve_comp : curve_lut_entities)
				{

					glDepthMask(GL_TRUE);
					glDepthFunc(GL_ALWAYS);
					glDisable(GL_BLEND);

					Component::Transform renderable_transform = curve_comp.Owner().GetComponent<Component::Transform>();
					glm::vec3 const renderable_pos = renderable_transform.ComputeWorldTransform().position;

					using curve_type = Component::piecewise_curve::EType;
					Component::piecewise_curve const& curve = curve_comp.GetPiecewiseCurve();

					for (unsigned int i = 0; i < curve.m_lut.m_points.size(); ++i)
					{
						node_transform.position = curve.m_lut.m_points[i] + renderable_pos;
						glm::mat4 const mesh_model_matrix = node_transform.GetMatrix();
						res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp * mesh_model_matrix);

						render_primitive(primitive);
					}
				}
			}
		}

		{
			res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_collider_debug"));
		
			set_bound_program_uniform_locations();

			int const LOC_HIGHLIGHT_INDEX = res_mgr.FindBoundProgramUniformLocation("u_highlight_index");

			// Debug convex hull rendering for physics objects
			using namespace Engine::Physics;
			auto const & collider_mgr = Singleton<Component::ColliderManager>();
			auto const& physics_mgr = Singleton<Engine::Physics::ScenePhysicsManager>();

			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			if (!collider_mgr.m_data.m_intersection_results.empty() && physics_mgr.render_contacts)
			{
				using namespace Component;
				using namespace Engine::Physics;
				using hds = half_edge_data_structure;

				// Debug Intersection rendering
				std::vector<glm::vec3> const & render_points = collider_mgr.m_data.m_global_contact_data.debug_draw_points;
				std::vector<glm::vec3> const & render_lines = collider_mgr.m_data.m_global_contact_data.debug_draw_lines;


				using namespace Engine::Graphics;
				using GraphicsManager = ResourceManager;
				auto const& gfx_mgr = Singleton<GraphicsManager>();
				auto const& line_point_prim_data = gfx_mgr.GetMeshPrimitives(
					s_pipeline_resources.line_point_mesh_handle
				);

				res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp);
				res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
				res_mgr.SetBoundProgramUniform(LOC_HIGHLIGHT_INDEX, (int)-1);

				glBindVertexArray(line_point_prim_data.front().m_vao_gl_id);

				if (!render_points.empty())
				{
					glPointSize(10.0f);
					s_pipeline_resources.upload_line_point_data(render_points.data(), render_points.size());
					glDrawArrays(GL_POINTS, 0, render_points.size());
				}
				if (!render_lines.empty())
				{
					glLineWidth(8.0f);
					s_pipeline_resources.upload_line_point_data(render_lines.data(), render_lines.size());
					glDrawArrays(GL_LINES, 0, render_lines.size());
				}
				glBindVertexArray(0);
			}
			auto const & debug_resolved_contacts = collider_mgr.m_data.m_global_contact_data.debug_resolved_contacts;
			if (!debug_resolved_contacts.empty())
			{
				using namespace Engine::Graphics;
				using GraphicsManager = ResourceManager;
				auto const& gfx_mgr = Singleton<GraphicsManager>();
				auto const& line_point_prim_data = gfx_mgr.GetMeshPrimitives(
					s_pipeline_resources.line_point_mesh_handle
				);

				res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp);
				res_mgr.SetBoundProgramUniform(LOC_HIGHLIGHT_INDEX, (int)-1);

				glBindVertexArray(line_point_prim_data.front().m_vao_gl_id);
				glLineWidth(8.0f);

				float scale_lambda_normals = physics_mgr.scale_lambda_resolution_vectors;
				if (physics_mgr.render_penetration)
				{
					std::vector<glm::vec3> penetration_points(debug_resolved_contacts.size() * 2);
					for (auto const contact : debug_resolved_contacts)
					{
						penetration_points.emplace_back(contact.contact.point);
						penetration_points.emplace_back(contact.contact.point + contact.contact.normal * contact.contact.penetration);
					}
					res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
					s_pipeline_resources.upload_line_point_data(penetration_points.data(), penetration_points.size());
					glDrawArrays(GL_LINES, 0, penetration_points.size());
				}
				if (physics_mgr.render_penetration_resolution)
				{
					std::vector<glm::vec3> penetration_points(debug_resolved_contacts.size()*2);
					for (auto const contact : debug_resolved_contacts)
					{
						penetration_points.emplace_back(contact.contact.point);
						penetration_points.emplace_back(contact.contact.point + contact.contact.normal * contact.contact_lambdas.lambda_penetration * scale_lambda_normals);
					}
					res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
					s_pipeline_resources.upload_line_point_data(penetration_points.data(), penetration_points.size());
					glDrawArrays(GL_LINES, 0, penetration_points.size());
				}
				if (physics_mgr.render_friction_resolution)
				{
					std::vector<glm::vec3> u_points(debug_resolved_contacts.size() * 2);
					std::vector<glm::vec3> v_points(debug_resolved_contacts.size() * 2);
					for (auto const contact : debug_resolved_contacts)
					{
						u_points.emplace_back(contact.contact.point);
						u_points.emplace_back(contact.contact.point + contact.vec_u * contact.contact_lambdas.lambda_friction_u * scale_lambda_normals);
						v_points.emplace_back(contact.contact.point);
						v_points.emplace_back(contact.contact.point + contact.vec_v * contact.contact_lambdas.lambda_friction_v * scale_lambda_normals);

						res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
						s_pipeline_resources.upload_line_point_data(u_points.data(), v_points.size());
						glDrawArrays(GL_LINES, 0, u_points.size());

						res_mgr.SetBoundProgramUniform(LOC_BASE_COLOR_FACTOR, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
						s_pipeline_resources.upload_line_point_data(v_points.data(), v_points.size());
						glDrawArrays(GL_LINES, 0, v_points.size());
					}


				}
				
				glBindVertexArray(0);
			}

			glLineWidth(2.0f);
			for (auto [e, ch_handle] : collider_mgr.m_data.m_entity_map)
			{
				bool set_intersection_base_color = false;

				Component::Transform transform = e.GetComponent<Component::Transform>();
				Engine::Math::transform3D world_transform = transform.ComputeWorldTransform();


				auto const& e_ch_debug_render_instance = collider_mgr.m_data.m_entity_map.at(e);
				Engine::Managers::Resource const ch_collider_res = e_ch_debug_render_instance.m_collider_resource;
				convex_hull_handle const ch_handle = ch_collider_res.Handle();
				if (ch_collider_res.ID() == 0 || ch_handle == 0)
					continue;

				auto it = collider_mgr.m_data.m_ch_debug_meshes.find(ch_collider_res);
				if (it == collider_mgr.m_data.m_ch_debug_meshes.end())
					continue;

				auto const& intersection_data = collider_mgr.m_data.m_entity_intersections;
				auto e_intersect_it = intersection_data.find(e);
				if (e_intersect_it != intersection_data.end())
				{
					set_intersection_base_color = true;
				}

				auto debug_render_data = it->second;
				res_mgr.SetBoundProgramUniform(LOC_MAT_MVP, matrix_vp * world_transform.GetMatrix());

				if (collider_mgr.m_data.m_render_debug_face_mesh && debug_render_data.m_ch_face_mesh)
				{
					auto& primitive_list = res_mgr.GetMeshPrimitives(debug_render_data.m_ch_face_mesh);
					for (auto& primitive : primitive_list)
					{
						glm::vec4 use_base_color;
						if (set_intersection_base_color && false /*Disable coloring on collision*/)
							use_base_color = glm::vec4(0.0f, 1.0f, 0.0f, 0.75f);
						else
							use_base_color = res_mgr.GetMaterial(primitive.m_material_handle).m_pbr_metallic_roughness.m_base_color_factor;
						res_mgr.SetBoundProgramUniform(
							LOC_BASE_COLOR_FACTOR,
							use_base_color
						);
						res_mgr.SetBoundProgramUniform(LOC_HIGHLIGHT_INDEX, e_ch_debug_render_instance.m_highlight_face_index);
						render_primitive(primitive);
					}
				}
				if (collider_mgr.m_data.m_render_debug_edge_mesh && debug_render_data.m_ch_edge_mesh)
				{
					auto& primitive_list = res_mgr.GetMeshPrimitives(debug_render_data.m_ch_edge_mesh);
					for (auto& primitive : primitive_list)
					{
						res_mgr.SetBoundProgramUniform(
							LOC_BASE_COLOR_FACTOR,
							res_mgr.GetMaterial(primitive.m_material_handle).m_pbr_metallic_roughness.m_base_color_factor
						);
						res_mgr.SetBoundProgramUniform(LOC_HIGHLIGHT_INDEX, e_ch_debug_render_instance.m_highlight_edge_index);
						render_primitive(primitive);
					}
				}
			}

		}


		// # 
		// # Cascading Shadow Map Rendering
		// # 

		ubo_cascading_shadow_map_data csm_data;
		if (Singleton<Component::DirectionalLightManager>().GetDirectionalLight().IsValid())
		{
			csm_data = Sandbox::RenderDirectionalLightCSM(camera_component.GetCameraData(), cam_transform);
			Sandbox::RenderShadowMapToFrameBuffer(camera_component.Owner(), window_size, csm_data, s_fb_texture_depth, s_fb_texture_normal, s_framebuffer_shadow);
		}

		// Lighting Stage
		res_mgr.BindFramebuffer(s_framebuffer_lighting);
		GLenum const draw_fb_lighting_attachment_01[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		res_mgr.DrawFramebuffers(s_framebuffer_lighting, 2, draw_fb_lighting_attachment_01);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(0, 0, window_size.x, window_size.y);
		glDisable(GL_BLEND);

		res_mgr.UseProgram(res_mgr.FindShaderProgram("draw_phong"));

		set_bound_program_uniform_locations();
		int LOC_SHININESS_MULT_FACTOR = res_mgr.FindBoundProgramUniformLocation("u_shininess_mult_factor");
		int LOC_BLOOM_TRESHHOLD = res_mgr.FindBoundProgramUniformLocation("u_bloom_treshhold");

		res_mgr.SetBoundProgramUniform(LOC_SHININESS_MULT_FACTOR, s_shininess_mult_factor);
		res_mgr.SetBoundProgramUniform(LOC_BLOOM_TRESHHOLD, s_bloom_treshhold_color);

		activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEPTH, 0);
		activate_texture(s_fb_texture_base_color, LOC_SAMPLER_BASE_COLOR, 1);
		activate_texture(s_fb_texture_metallic_roughness, LOC_SAMPLER_METALLIC, 2);
		activate_texture(s_fb_texture_normal, LOC_SAMPLER_NORMAL, 3);

		mesh_handle sphere_mesh = res_mgr.FindMesh("Sphere/Sphere");
		if (sphere_mesh)
		{
			Sandbox::RenderPointLights(sphere_mesh, camera_component.GetCameraData(), cam_transform);
		}

		//
		//	Volumetric Fog
		//
		pipeline_volumetric_fog(cam_transform, camera_data);

		/////////// Global Lighting (Ambient, Directional Light, apply Volumetric Fog) //////////////////

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		glDepthMask(GL_TRUE);
		glCullFace(GL_BACK);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);

		Component::DirectionalLight const dl = Singleton<Component::DirectionalLightManager>().GetDirectionalLight();
		res_mgr.UseProgram(program_draw_global_light);
		set_bound_program_uniform_locations();
		int LOC_SAMPLER_SHADOWMAP = res_mgr.FindBoundProgramUniformLocation("u_sampler_shadow");
		int LOC_SAMPLER_AO = res_mgr.FindBoundProgramUniformLocation("u_sampler_ao");
		int LOC_SAMPLER_VOLFOG = res_mgr.FindBoundProgramUniformLocation("u_sampler_volumetric_fog");
		int LOC_AMBIENT_COLOR = res_mgr.FindBoundProgramUniformLocation("u_ambient_color");
		int LOC_CSM_RENDER_CASCADES = res_mgr.FindBoundProgramUniformLocation("u_csm_render_cascades");

		int LOC_UBO_CSM_DATA = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, 
			"ubo_csm_data"
		);
		GLuint const LOC_UBO_FOGCAM = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, "ubo_fogcam"
		);
		GLuint const LOC_UBO_CAMERA = glGetUniformBlockIndex(
			res_mgr.m_bound_gl_program_object, "ubo_camera_data"
		);
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_FOGCAM,
			ubo_volfog_camera::BINDING_POINT
		);
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_CAMERA,
			ubo_camera_data::BINDING_POINT
		);
		// Bind CSM data UBO
		glUniformBlockBinding(
			res_mgr.m_bound_gl_program_object,
			LOC_UBO_CSM_DATA,
			ubo_cascading_shadow_map_data::BINDING_POINT
		);

		res_mgr.SetBoundProgramUniform(LOC_AMBIENT_COLOR, s_ambient_color);
		res_mgr.SetBoundProgramUniform(LOC_CSM_RENDER_CASCADES, dl.IsValid() & dl.GetCascadeDebugRendering());
		if(LOC_SAMPLER_DEPTH != -1)
			activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEPTH, 0);
		activate_texture(s_fb_texture_base_color, LOC_SAMPLER_BASE_COLOR, 1);
		activate_texture(s_fb_texture_ao_pingpong, LOC_SAMPLER_AO, 2);
		activate_texture(s_volumetric_fog_pipeline_data.m_volumetric_accumulation_texture, LOC_SAMPLER_VOLFOG, 3);
		if(LOC_SAMPLER_SHADOWMAP != -1)
			activate_texture(dl.IsValid() ? s_fb_texture_shadow : s_texture_white, LOC_SAMPLER_SHADOWMAP, 4);

		// Disable drawing to luminance buffer when computing ambient color.
		GLenum const draw_fb_lighting_attachment_0[] = { GL_COLOR_ATTACHMENT0 };
		res_mgr.DrawFramebuffers(s_framebuffer_lighting, 1, draw_fb_lighting_attachment_0);

		GfxCall(glBindVertexArray(0));
		GfxCall(glDrawArrays(GL_TRIANGLES, 0, 3));


		/////////// Bloom //////////////

		if (s_bloom_enabled)
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glDepthMask(GL_FALSE);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);

			shader_program_handle bloom_blur_program = res_mgr.FindShaderProgram("process_bloom_blur");
			res_mgr.UseProgram(bloom_blur_program);
			set_bound_program_uniform_locations();

			int LOC_SAMPLER_BLOOM_INPUT = res_mgr.GetBoundProgramUniformLocation("u_sampler_bloom_input");
			int LOC_BOOL_BLUR_HORIZONTAL = res_mgr.GetBoundProgramUniformLocation("u_bool_blur_horizontal");

			Engine::Graphics::ResourceManager::texture_info luminance_tex_info = res_mgr.GetTextureInfo(s_fb_texture_luminance);

			glBindTexture(luminance_tex_info.m_target, luminance_tex_info.m_gl_source_id);
			glGenerateMipmap(luminance_tex_info.m_target);

			GfxCall(glBindVertexArray(0));

			bool first_iteration = true;
			for (unsigned int i = 0; i < s_bloom_blur_count; ++i)
			{
				auto use_bloom_input_texture = first_iteration ? s_fb_texture_luminance : s_fb_texture_bloom_pingpong[(i + 1) % 2];
				res_mgr.BindFramebuffer(s_framebuffer_bloom[i % 2]);
				activate_texture(use_bloom_input_texture, LOC_SAMPLER_BLOOM_INPUT, 0);

				int texture_gl_id = res_mgr.GetTextureInfo(use_bloom_input_texture).m_gl_source_id;
				glViewport(0, 0, s_bloom_texture_size.x, s_bloom_texture_size.y);

				res_mgr.SetBoundProgramUniform(LOC_BOOL_BLUR_HORIZONTAL, (int)((i % 2) == 0));

				GfxCall(glDrawArrays(GL_TRIANGLES, 0, 3));
				first_iteration = false;

			}
		}

		// Render onto final framebuffer
		glViewport(0, 0, (GLsizei)window_size.x, (GLsizei)window_size.y);

		res_mgr.UnbindFramebuffer();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glEnable(GL_CULL_FACE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		glDisable(GL_BLEND);

		if (s_ambient_occlusion.render_mode == GfxAmbientOcclusion::RenderMode::eAO_Applied)
		{
			shader_program_handle const postprocessing_program = res_mgr.FindShaderProgram("postprocessing");

			res_mgr.UseProgram(postprocessing_program);

			set_bound_program_uniform_locations();

			int LOC_SAMPLER_SCENE = res_mgr.GetBoundProgramUniformLocation("u_sampler_scene");
			int LOC_SAMPLER_BLOOM = res_mgr.GetBoundProgramUniformLocation("u_sampler_bloom");
			int LOC_EXPOSURE = res_mgr.GetBoundProgramUniformLocation("u_exposure");
			int LOC_GAMMA = res_mgr.GetBoundProgramUniformLocation("u_gamma");

			activate_texture(s_fb_texture_light_color, LOC_SAMPLER_SCENE, 0);
			activate_texture(s_fb_texture_bloom_pingpong[1], LOC_SAMPLER_BLOOM, 1);
			activate_texture(s_fb_texture_depth, LOC_SAMPLER_DEPTH, 2);

			res_mgr.SetBoundProgramUniform(LOC_EXPOSURE, s_exposure);
			res_mgr.SetBoundProgramUniform(LOC_GAMMA, s_gamma_correction_factor);

			GfxCall(glBindVertexArray(0));
			GfxCall(glDrawArrays(GL_TRIANGLES, 0, 3));

			if (s_render_infinite_grid)
			{
				// Render endless grid
				auto program = res_mgr.FindShaderProgram("draw_infinite_grid");
				res_mgr.UseProgram(program);
				res_mgr.SetBoundProgramUniform(0, cam_transform.GetInvMatrix());
				res_mgr.SetBoundProgramUniform(1, camera_component.GetCameraData().get_perspective_matrix());
				res_mgr.SetBoundProgramUniform(10, camera_component.GetNearDistance());
				res_mgr.SetBoundProgramUniform(11, camera_component.GetFarDistance());

				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LEQUAL);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}
		else
		{
			// Render AO texture with bilateral-filter blurring applied.
			shader_program_handle const render_ao_texture = res_mgr.FindShaderProgram("draw_framebuffer_ao_monochrome");
			res_mgr.UseProgram(render_ao_texture);
			set_bound_program_uniform_locations();
			activate_texture(s_fb_texture_ao_pingpong, LOC_SAMPLER_BASE_COLOR, 0);

			GfxCall(glDrawArrays(GL_TRIANGLES, 0, 3));
		}

	}
}

void gfx_pipeline_resources::upload_line_point_data(glm::vec3 const* _points, size_t _count) const
{
	using namespace Engine::Graphics;
	using GraphicsManager = ResourceManager;
	auto const& gfx_mgr = Singleton<GraphicsManager>();
	
	assert(_count <= MAX_POINT_COUNT);
	GraphicsManager::buffer_info const & buf_info = gfx_mgr.GetBufferInfo(s_pipeline_resources.line_point_mesh_vbo_handle);
	glBindBuffer(buf_info.m_target, buf_info.m_gl_id);
	glBufferSubData(buf_info.m_target, (GLintptr)0, sizeof(glm::vec3) * _count, _points);
}
