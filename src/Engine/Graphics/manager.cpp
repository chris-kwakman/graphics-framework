#include "manager.h"

#include <limits>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <set>

#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL.h>
#include <Engine/Utils/logging.h>

#include <STB/stb_image.h>
#include <tiny_glft/tiny_gltf.h>


namespace Engine {
namespace Graphics {

	void GLClearError()
	{
		if (SDL_GL_GetCurrentContext() == 0) return;
		while (glGetError() != GL_NO_ERROR);
	}

	bool GLLogCall(const char* _fn, const char* _file, int _line)
	{
		bool bNoErrors{ true };
		if (SDL_GL_GetCurrentContext() == 0) return false;
		while (GLenum error = glGetError())
		{
			if (error == GL_NO_ERROR)
				break;

			Utils::print_error("OpenGL Error (%X) in %s (%s, line %u)", error, _fn, _file, _line);

			bNoErrors = false;
		}
		return bNoErrors;
	}

	unsigned int ResourceManager::get_gl_component_size(GLuint _componentType)
	{
		switch (_componentType)
		{
		case GL_BYTE:				return 1;
		case GL_UNSIGNED_BYTE:		return 1;
		case GL_SHORT:				return 2;
		case GL_UNSIGNED_SHORT:		return 2;
		case GL_UNSIGNED_INT:		return 4;
		case GL_INT:				return 4;
		case GL_FLOAT:				return 4;
		}
		assert(false && "OpenGL component type not supported.");
		return 0;
	}

	unsigned int ResourceManager::get_glTF_type_component_count(int _attributeType)
	{
		switch (_attributeType)
		{
		case TINYGLTF_TYPE_SCALAR:	return 1;
		case TINYGLTF_TYPE_VEC2:	return 2;
		case TINYGLTF_TYPE_VEC3:	return 3;
		case TINYGLTF_TYPE_VEC4:	return 4;
		case TINYGLTF_TYPE_MAT2:	return 4;
		case TINYGLTF_TYPE_MAT3:	return 9;
		case TINYGLTF_TYPE_MAT4:	return 16;
		}
		assert(false && "Attribute type not supported.");
		return 0;
	}


	bool ResourceManager::LoadModel(const char* _filepath)
	{
		std::filesystem::path const path(_filepath);
		std::error_code err;
		bool success = std::filesystem::exists(path, err);
		if (success)
		{
			success = false;
			std::filesystem::path const extension = path.extension();
			if(extension == ".gltf")
				success = load_gltf_model(_filepath);
			else
				printf("Extension \"%s\" is not supported.\n", extension.u8string().c_str());
		}
		else
		{
			printf("Error reading file: %s.\n", err.message().c_str());
		}
		return success;
	}

	bool ResourceManager::load_gltf_model(const char* _filepath)
	{
		using namespace tinygltf;

		Model model;

		TinyGLTF loader;
		std::string error, warning;

		bool success = loader.LoadASCIIFromFile(&model, &error, &warning, _filepath);

		if (!warning.empty()) {
			printf("[TinyGLTF] Warn: %s\n", warning.c_str());
		}

		if (!error.empty()) {
			printf("[TinyGLTF] Error: %s\n", error.c_str());
		}

		if (!success) {
			printf("[TinyGLTF] Failed to parse glTF\n");
			return false;
		}

		/*
				Load gltf model into internal manager.
		*/

		//decltype(m_node_data_map) new_node_data_map;
		//decltype(m_node_mesh_map) new_node_mesh_map;
		decltype(m_buffer_info_map) new_buffer_info_map;
		decltype(m_index_buffer_info_map) new_index_buffer_info_map;
		decltype(m_mesh_primitives_map) new_mesh_primitives_map;

		/*
		* Load buffers (VBOs & IBOs)
		*/

		// Generate all GL buffers at once.
		unsigned int const new_bufferview_count = (unsigned int)model.bufferViews.size();
		std::vector<GLuint> new_gl_buffer_arr;
		new_gl_buffer_arr.resize(new_bufferview_count);
		glGenBuffers(new_bufferview_count, &new_gl_buffer_arr[0]);
		// Assert that all generated buffers are valid
		for (unsigned int i = 0; i < new_gl_buffer_arr.size(); ++i)
			assert(
				new_gl_buffer_arr[i] != GL_INVALID_VALUE &&
				"OpenGL could not generate all requested buffers."
			);

		unsigned int gpu_buffer_count = 0;
		for (unsigned int i = 0; i < new_bufferview_count; ++i)
		{
			buffer_handle const curr_new_handle = m_buffer_handle_counter + gpu_buffer_count;	
			tinygltf::BufferView const& read_bufferview = model.bufferViews[i];
			tinygltf::Buffer const& read_buffer = model.buffers[read_bufferview.buffer];

			if (read_bufferview.target == 0)
			{
				Engine::Utils::print_warning("glTF Buffer View (%u) Target is undefined.", i);
				continue;
			}

			buffer_info new_buffer_info;
			new_buffer_info.m_gl_id = new_gl_buffer_arr[i];
			new_buffer_info.m_byte_length = read_bufferview.byteLength;
			new_buffer_info.m_target = read_bufferview.target;
			new_buffer_info.m_byte_stride = read_bufferview.byteStride;

			// Create buffer memory block & upload data
			glBindBuffer(new_buffer_info.m_target, new_buffer_info.m_gl_id);
			// glTF buffers do not interleave binary blobs, so we can upload directly.
			GfxCall(glBufferData(
				new_buffer_info.m_target, new_buffer_info.m_byte_length, 
				static_cast<GLvoid const*>(&read_buffer.data[read_bufferview.byteOffset]), 
				GL_STATIC_DRAW
			));

			new_buffer_info_map.emplace(curr_new_handle, new_buffer_info);
			gpu_buffer_count++;
		}

		/*
		* Set up meshes (lists of VAOs)
		*/

		unsigned int const new_mesh_count = (unsigned int)model.meshes.size();
		
		for (unsigned int i = 0; i < new_mesh_count; ++i)
		{
			tinygltf::Mesh const& read_mesh = model.meshes[i];

			std::vector<mesh_primitive_data> curr_mesh_primitives;
			curr_mesh_primitives.resize(read_mesh.primitives.size());

			// Create a VAO for each primitive in the current mesh
			std::vector<GLuint> new_gl_vao_array;
			new_gl_vao_array.resize(read_mesh.primitives.size());
			GfxCall(glGenVertexArrays((unsigned int)read_mesh.primitives.size(), &new_gl_vao_array[0]));
			// Assert that all generated VAOs are valid
			for (unsigned int i = 0; i < new_gl_vao_array.size(); ++i)
				assert(
					new_gl_vao_array[i] != GL_INVALID_VALUE &&
					"OpenGL could not generate all requested buffers."
				);

			// Bind IBO and all VBOs of buffers referenced by primitive attributes
			for (unsigned int p = 0; p < read_mesh.primitives.size(); ++p)
			{
				tinygltf::Primitive const& read_primitive = read_mesh.primitives[p];
				mesh_primitive_data& new_primitive = curr_mesh_primitives[p];

				new_primitive.m_vao_gl_id = new_gl_vao_array[p];
				new_primitive.m_material_handle = m_material_handle_counter + read_primitive.material;
				new_primitive.m_index_buffer_handle = m_buffer_handle_counter + read_primitive.indices;
				new_primitive.m_render_mode = read_primitive.mode;

				glBindVertexArray(new_primitive.m_vao_gl_id);

				// Bind respective IBO (assume IBO exists for each primitive)
				buffer_info const primitive_index_buffer = new_buffer_info_map.at(new_primitive.m_index_buffer_handle);
				glBindBuffer(primitive_index_buffer.m_target, primitive_index_buffer.m_gl_id);
				// Insert component type of IBO into index buffer component map if it hasn't been done already
				// Overwriting pre-existing index buffer info element is fine since it should be a copy anyways.
				index_buffer_info new_index_buffer_info;
				new_index_buffer_info.m_type = model.accessors[read_primitive.indices].componentType;
				new_index_buffer_info.m_index_count = model.accessors[read_primitive.indices].count;
				new_index_buffer_info_map.emplace(new_primitive.m_index_buffer_handle, new_index_buffer_info);

				//Engine::Utils::print_debug("Bind VAO %u", new_primitive.m_vao_gl_id);
				//Engine::Utils::print_debug("\tBind IBO %u", primitive_index_buffer.m_gl_id);

				// Bind respective VBOs referred to by primitive attributes + index buffer

				for (auto const & primitive_attrib : read_primitive.attributes)
				{
					// Determine location of attribute in shader.
					// Assume position, normal and tangent attributes always exist in primitives.

					//TODO: Support joints, weights (and colors?)
					unsigned int gl_attribute_index = 0;
					if (primitive_attrib.first == "POSITION") gl_attribute_index = 0;
					else if (primitive_attrib.first == "NORMAL") gl_attribute_index = 1;
					else if (primitive_attrib.first == "TANGENT") gl_attribute_index = 2;
					//TODO: Check if TEXCOORD actually exists rather than assuming it.
					else
					{
						unsigned int texcoord_index = 0;
						sscanf(primitive_attrib.first.c_str(), "TEXCOORD_%u", &texcoord_index);
						gl_attribute_index = 3 + texcoord_index;
					}

					// Get buffer pointed to by bufferView pointed to by current attribute in primitive
					tinygltf::Accessor const& read_accessor = model.accessors[primitive_attrib.second];
					tinygltf::BufferView const& read_bufferview = model.bufferViews[read_accessor.bufferView];
					buffer_handle const attribute_buffer_handle =
						m_buffer_handle_counter + read_accessor.bufferView;
					buffer_info const attribute_referenced_buffer = new_buffer_info_map.at(attribute_buffer_handle);
					glBindBuffer(attribute_referenced_buffer.m_target, attribute_referenced_buffer.m_gl_id);
					glEnableVertexArrayAttrib(new_primitive.m_vao_gl_id, gl_attribute_index);
					glVertexAttribPointer(
						gl_attribute_index,
						get_glTF_type_component_count(read_accessor.type),
						read_accessor.componentType,
						read_accessor.normalized,
						read_accessor.ByteStride(read_bufferview),
						(GLvoid*)(read_accessor.byteOffset)
					);

					//Engine::Utils::print_debug("\tBind VBO %u", attribute_referenced_buffer.m_gl_id);
					//Engine::Utils::print_debug("\tEnable attribute %u (%s)", gl_attribute_index, primitive_attrib.first);

					gl_attribute_index++;
				}
				GfxCall(glBindVertexArray(0));
			}
			new_mesh_primitives_map.emplace(m_mesh_handle_counter + i, std::move(curr_mesh_primitives));
		}

		/*
		* Textures / Images / Samplers
		*/

		// Insert glTF texture information into internal map.
		decltype(m_texture_info_map) new_texture_info_map;
		for (unsigned int i = 0; i < model.textures.size(); ++i)
		{
			texture_info new_texture_info;
			tinygltf::Texture const & read_texture = model.textures[i];
			//new_texture_info.m_sampler_handle = m_texture_sampler_handle_counter + read_texture.sampler;
			new_texture_info.m_source_handle = m_texture_source_handle_counter + read_texture.source;
			new_texture_info_map.emplace(m_texture_handle_counter + i, new_texture_info);
		}

		// Add glTF sampler information into internal map.
		/*decltype(m_texture_sampler_info_map) new_texture_sampler_info_map;
		for (unsigned int i = 0; i < model.samplers.size(); ++i)
		{
			texture_sampler_info new_texture_sampler_info;
			tinygltf::Sampler const read_texture_sampler = model.samplers[i];
			new_texture_sampler_info.m_wrap_s_enum = read_texture_sampler.wrapS;
			new_texture_sampler_info.m_wrap_t_enum = read_texture_sampler.wrapT;
			new_texture_sampler_info.m_mag_filter_enum = read_texture_sampler.magFilter;
			new_texture_sampler_info.m_min_filter_enum = read_texture_sampler.minFilter;
			new_texture_sampler_info_map.emplace(m_texture_sampler_handle_counter + i, new_texture_sampler_info);
		}
		*/

		// Add glTF source information into internal map.
		decltype(m_texture_source_info_map) new_texture_source_info_map;

		// Generate openGL textures beforehand
		std::vector<GLuint> new_gl_texture_objects;
		new_gl_texture_objects.resize(model.images.size());
		glGenTextures((GLsizei)new_gl_texture_objects.size(), &new_gl_texture_objects[0]);

		for (unsigned int i = 0; i < model.images.size(); ++i)
		{
			texture_source_info new_texture_source_info;
			tinygltf::Image & read_texture_source = model.images[i];
			new_texture_source_info.m_gl_source_id = new_gl_texture_objects[i];
			new_texture_source_info.m_target = GL_TEXTURE_2D; // Assume all glTF textures are 2D.

			// First, texture data must be loaded into memory

			unsigned char* image_data = nullptr;
			// Image is directly stored.
			if (read_texture_source.image.empty())
				image_data = (unsigned char*)&read_texture_source.image[0];
			// Image is stored in either bufferView or externally.
			else
			{
				// Load from file if URI is defined
				if (!read_texture_source.uri.empty())
				{
					std::filesystem::path path(_filepath);
					path.remove_filename();
					path = path.append(read_texture_source.uri);
					assert(std::filesystem::exists(path));
					
					image_data = stbi_load(
						path.string().c_str(),
						&read_texture_source.width,
						&read_texture_source.height,
						&read_texture_source.component,
						0
					);
				}
				// Load from BufferView if URI is not defined
				else
				{
					// Assume bufferview data for image is contiguous
					tinygltf::BufferView const& read_bufferview = model.bufferViews[read_texture_source.bufferView];
					stbi_uc* image_ptr = (stbi_uc*)&model.buffers[read_bufferview.buffer].data[read_bufferview.byteOffset];
					image_data = stbi_load_from_memory(
						image_ptr,
						(int)read_texture_source.image.size(),
						&read_texture_source.width,
						&read_texture_source.height,
						&read_texture_source.component,
						0
					);
				}
			}

			// Bind texture source and set parameters
			GfxCall(glBindTexture(GL_TEXTURE_2D, new_texture_source_info.m_gl_source_id));
			GLenum source_format;
			GLenum pixel_component_type;
			switch (read_texture_source.component)
			{
			case 1:		source_format = GL_R; break;
			case 2:		source_format = GL_RG; break;
			case 3:		source_format = GL_RGB; break;
			case 4:		source_format = GL_RGBA; break;
			}
			switch (read_texture_source.bits)
			{
			case 8:		pixel_component_type = GL_UNSIGNED_BYTE; break;
			case 16:	pixel_component_type = GL_UNSIGNED_SHORT; break;
			case 32:	pixel_component_type = GL_UNSIGNED_INT; break;
			}
			
			GfxCall(glTexImage2D(
				GL_TEXTURE_2D,
				0,
				source_format,
				read_texture_source.width, read_texture_source.height,
				0,
				source_format,
				pixel_component_type,
				image_data
			));
			GfxCall(glGenerateMipmap(GL_TEXTURE_2D));
			GfxCall(glBindTexture(GL_TEXTURE_2D, 0));


			// Free allocated memory (if any)
			if(image_data)
				stbi_image_free(image_data);

			new_texture_source_info_map.emplace(m_texture_source_handle_counter + i, new_texture_source_info);
		}

		// Go through textures from samplers one more time and set parameters properly.
		for (unsigned int i = 0; i < model.textures.size(); ++i)
		{
			tinygltf::Texture const& read_texture = model.textures[i];
			tinygltf::Sampler const& read_sampler = model.samplers[read_texture.sampler];

			texture_handle current_texture = m_texture_handle_counter + i;
			texture_source_handle current_source = new_texture_info_map.at(current_texture).m_source_handle;
			texture_source_info current_source_info = new_texture_source_info_map.at(current_source);
			glBindTexture(GL_TEXTURE_2D, current_source_info.m_gl_source_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, read_sampler.wrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, read_sampler.wrapT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, read_sampler.minFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, read_sampler.magFilter);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		/*
		*	Materials
		*/

		//TODO: Load material data into new map
		decltype(m_material_data_map) new_material_data_map;
		for (unsigned int i = 0; i < model.materials.size(); ++i)
		{
			tinygltf::Material const& read_material = model.materials[i];
			tinygltf::PbrMetallicRoughness const read_pbr_mr = read_material.pbrMetallicRoughness;

			material_handle new_material_handle = m_material_handle_counter + i;
			material_data new_material_data;

			// Set up PBR metallic roughness data
			pbr_metallic_roughness_data new_pbr_mrd;
			memcpy(
				new_pbr_mrd.m_base_color_factor, 
				&read_pbr_mr.baseColorFactor.front(), 
				sizeof(pbr_metallic_roughness_data::m_base_color_factor)
			);
			new_pbr_mrd.m_metallic_factor = read_pbr_mr.metallicFactor;
			new_pbr_mrd.m_roughness_factor = read_pbr_mr.roughnessFactor;
			new_pbr_mrd.m_texture_base_color = m_texture_handle_counter + read_pbr_mr.baseColorTexture.index;
			new_pbr_mrd.m_texture_metallic_roughness = m_texture_handle_counter + read_pbr_mr.metallicRoughnessTexture.index;

			// Setup material data
			new_material_data.m_pbr_metallic_roughness = new_pbr_mrd;
			new_material_data.m_texture_normal = m_texture_handle_counter + read_material.normalTexture.index;
			new_material_data.m_texture_occlusion = m_texture_handle_counter + read_material.occlusionTexture.index;
			new_material_data.m_texture_emissive = m_texture_handle_counter + read_material.emissiveTexture.index;

			using material_alpha_mode = material_data::alpha_mode;
			material_alpha_mode new_material_alpha_mode = material_alpha_mode::eOPAQUE;
			if (read_material.alphaMode == "OPAQUE") new_material_alpha_mode = material_alpha_mode::eOPAQUE;
			else if (read_material.alphaMode == "MASK") new_material_alpha_mode = material_alpha_mode::eMASK;
			else if (read_material.alphaMode == "BLEND") new_material_alpha_mode = material_alpha_mode::eBLEND;
			new_material_data.m_alpha_mode = new_material_alpha_mode;
			new_material_data.m_alpha_cutoff = read_material.alphaCutoff;
			new_material_data.m_double_sided = read_material.doubleSided;
			new_material_data.m_name = read_material.name;
			
			new_material_data_map.emplace(new_material_handle, new_material_data);
		}


		/*
		* Load Nodes & corresponding data
		*/
		
		//unsigned int new_meshes = 0;

		//for (unsigned int i = 0; i < model.nodes.size(); ++i)
		//{
		//	node_handle const curr_new_node_handle = m_node_handle_counter + i;

		//	tinygltf::Node const& read_node = model.nodes[i];
		//	node_data new_data;

		//	new_data.m_name = read_node.name;

		//	// Copy child node handles for currently read nodes
		//	new_data.m_child_nodes.resize(read_node.children.size());
		//	for (unsigned int j = 0; j < read_node.children.size(); ++j)
		//		new_data.m_child_nodes[j] = m_node_handle_counter + read_node.children[j];

		//	// Set type of data that is contained in new node, and add to respective map.
		//	if (read_node.mesh != -1)
		//	{
		//		new_data.m_contained_data = node_data::eMesh;
		//		new_node_mesh_map.emplace(curr_new_node_handle, m_mesh_handle_counter + read_node.mesh);
		//		new_meshes++;
		//	}
		//	//else if (read_node.camera != -1)
		//	//{
		//	//	new_data.m_contained_data = node_data::eCamera;
		//	//	new_node_camera_map.emplace(curr_new_node_handle, new_camera_first_handle + read_node.camera);
		//	//}
		//	else
		//		assert(true && "Node type not supported.");
		//	
		//	// Set node transform (if found)
		//	if (read_node.matrix.empty())
		//	{
		//		if (!read_node.translation.empty())
		//		{
		//			new_data.m_transform.position.x = (float)read_node.translation[0];
		//			new_data.m_transform.position.y = (float)read_node.translation[1];
		//			new_data.m_transform.position.z = (float)read_node.translation[2];

		//		}
		//		if (!read_node.scale.empty())
		//		{
		//			new_data.m_transform.scale.x = (float)read_node.scale[0];
		//			new_data.m_transform.scale.y = (float)read_node.scale[1];
		//			new_data.m_transform.scale.z = (float)read_node.scale[2];
		//		}
		//		if (!read_node.rotation.empty())
		//		{
		//			new_data.m_transform.quaternion.x = (float)read_node.rotation[0];
		//			new_data.m_transform.quaternion.y = (float)read_node.rotation[1];
		//			new_data.m_transform.quaternion.z = (float)read_node.rotation[2];
		//			new_data.m_transform.quaternion.w = (float)read_node.rotation[3];
		//		}
		//	}
		//	else
		//	{
		//		assert(true && "GLTF model loading does not support decomposable matrix transforms right now.");
		//	}

		//	new_node_data_map.emplace(curr_new_node_handle, std::move(new_data));
		//}

		//TODO: Load scene data into new map

		
		m_mesh_handle_counter += (unsigned int)new_mesh_primitives_map.size();
		m_buffer_handle_counter += (unsigned int)new_buffer_info_map.size();
		m_material_handle_counter += (unsigned int)new_material_data_map.size();
		m_texture_handle_counter += (unsigned int)new_texture_info_map.size();
		m_texture_source_handle_counter += (unsigned int)new_texture_source_info_map.size();
		//m_node_handle_counter += (unsigned int)model.nodes.size();

		m_buffer_info_map.merge(std::move(new_buffer_info_map));
		m_index_buffer_info_map.merge(std::move(new_index_buffer_info_map));
		m_mesh_primitives_map.merge(new_mesh_primitives_map);
		m_material_data_map.merge(new_material_data_map);
		m_texture_info_map.merge(new_texture_info_map);
		m_texture_source_info_map.merge(new_texture_source_info_map);
		//m_node_data_map.merge(new_node_data_map);
		//m_node_mesh_map.merge(new_node_mesh_map);

		return true;
	}

	//////////////////////////////////////////////////////////////////
	//						Shader Methods
	//////////////////////////////////////////////////////////////////

	std::vector<ResourceManager::shader_handle> ResourceManager::LoadShaders(std::vector<std::filesystem::path> _shader_paths)
	{
		std::vector<shader_handle> output_shader_handles;
		decltype(m_shader_info_map) new_shader_info_map;
		decltype(m_filepath_shader_map) new_filepath_shader_map;

		unsigned int new_shader_count = 0;

		for (unsigned int i = 0; i < _shader_paths.size(); ++i)
		{
			// Initialize new shader handle as invalid handle.
			shader_handle curr_output_shader_handle = std::numeric_limits<shader_handle>::max();

			// If filepath already exists within either new map or pre-existing map, return corresponding handle and
			// continue on to next shader path
			filepath_string const path_str(_shader_paths[i].string());
			auto iter = new_filepath_shader_map.find(path_str);
			if (iter != new_filepath_shader_map.end())
			{
				output_shader_handles.push_back(iter->second);
				continue;
			}
			iter = m_filepath_shader_map.find(path_str);
			if (iter != m_filepath_shader_map.end())
			{
				output_shader_handles.push_back(iter->second);
				continue;
			}

			// Check if we support shader extension
			filepath_string const extension = _shader_paths[i].extension().string();
			GLenum const shader_type = get_extension_shader_type(extension);
			Engine::Utils::assert_print_error(
				shader_type != GL_INVALID_ENUM,
				"Could not load shader \"%s\": Invalid extension \"%s\".",
				path_str.c_str(), extension.c_str()
			);
			shader_data* loaded_shader_data;
			if (!load_shader_data(path_str, &loaded_shader_data))
			{
				output_shader_handles.emplace_back(0);
				continue;
			}

			GLuint gl_shader_object;
			GfxCallReturn(gl_shader_object, glCreateShader(shader_type));
			Engine::Utils::assert_print_error(gl_shader_object != 0, "Failed to create shader object.");

			bool compilation_success = compile_gl_shader(gl_shader_object, loaded_shader_data, shader_type);

			shader_handle new_shader_handle = m_shader_handle_counter + new_shader_count;
			++new_shader_count;

			// Add shader object into shader info map regardless of compilation success.
			// In the event that we want to refresh shaders, the shader object will still
			// exist for recompilation.
			shader_info new_shader_info;
			new_shader_info.m_gl_shader_object = gl_shader_object;
			new_shader_info.m_programs_using_shader.clear();
			new_shader_info_map.emplace(new_shader_handle, std::move(new_shader_info));
			new_filepath_shader_map.emplace(path_str, new_shader_handle);
			output_shader_handles.push_back(new_shader_handle);

			unload_shader_data(loaded_shader_data);
		}

		m_shader_handle_counter += new_shader_count;

		m_filepath_shader_map.merge(new_filepath_shader_map);
		m_shader_info_map.merge(new_shader_info_map);

		return output_shader_handles;
	}

	ResourceManager::shader_program_handle ResourceManager::LoadShaderProgram(std::vector<std::filesystem::path> _shader_filepaths)
	{
		// Get shader handles corresponding to given shader filepaths from internal map
		std::vector<shader_handle> shader_handles;
		shader_handles.resize(_shader_filepaths.size());
		for (unsigned int i = 0; i < _shader_filepaths.size(); ++i)
			shader_handles[i] = m_filepath_shader_map.at(_shader_filepaths[i].string());

		return LoadShaderProgram(shader_handles);
	}

	void ResourceManager::RefreshShaders()
	{
		// Shader programs that need to be re-linked
		std::set<shader_program_handle> relink_shader_programs;

		// Recompile outdated shaders & submit programs that have attached outdated shaders for relinking.
		auto iter = m_filepath_shader_map.begin();
		while (iter != m_filepath_shader_map.end())
		{
			std::filesystem::path const & shader_file_path(iter->first);
			std::filesystem::file_time_type time = std::filesystem::last_write_time(shader_file_path);
			shader_info & shader_info = m_shader_info_map.at(iter->second);
			if (shader_info.m_last_time_written < time)
			{
				shader_info.m_last_time_written = time;
				ResourceManager::shader_data* loaded_shader_data = nullptr;
				if (load_shader_data(shader_file_path, &loaded_shader_data))
				{
					if (compile_gl_shader(
						shader_info.m_gl_shader_object, loaded_shader_data,
						get_extension_shader_type(shader_file_path.extension().string())
					))
					{
						relink_shader_programs.insert(shader_info.m_programs_using_shader.begin(), shader_info.m_programs_using_shader.end());
					}
					unload_shader_data(loaded_shader_data);
				}
			}
			++iter;
		}

		// Relink attached shaders
		for (shader_program_handle program : relink_shader_programs)
		{
			auto const & program_info = m_shader_program_info_map.at(program);
			link_gl_program_shaders(program_info.m_gl_program_object);
		}
	}

	void ResourceManager::UseProgram(shader_program_handle _program_handle)
	{
		if (m_bound_program != _program_handle)
		{
			shader_program_info const& program_info = m_shader_program_info_map.at(_program_handle);
			glUseProgram(program_info.m_gl_program_object);
			m_bound_program = _program_handle;
			m_bound_gl_program_object = program_info.m_gl_program_object;
		}
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, unsigned int _uniform_value)
	{
		GfxCall(glProgramUniform1ui(m_bound_gl_program_object, _uniform_location, _uniform_value));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, int _uniform_value)
	{
		GfxCall(glProgramUniform1i(m_bound_gl_program_object, _uniform_location, _uniform_value));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, float _uniform_value)
	{
		GfxCall(glProgramUniform1f(m_bound_gl_program_object, _uniform_location, _uniform_value));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec2 _uniform_value)
	{
		glProgramUniform2f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y);
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec3 _uniform_value)
	{
		glProgramUniform3f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y, _uniform_value.z);
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec4 _uniform_value)
	{
		glProgramUniform4f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y, _uniform_value.z, _uniform_value.w);
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::mat3 const & _uniform_value)
	{
		glProgramUniformMatrix3fv(m_bound_gl_program_object, _uniform_location, 1, false, glm::value_ptr(_uniform_value));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::mat4 const & _uniform_value)
	{
		glProgramUniformMatrix4fv(m_bound_gl_program_object, _uniform_location, 1, false, glm::value_ptr(_uniform_value));
	}

	ResourceManager::shader_program_handle ResourceManager::LoadShaderProgram(std::vector<shader_handle> _shader_handles)
	{

		// Convert shader handles to gl shader objects
		std::vector<GLuint> gl_shader_objects;
		gl_shader_objects.resize(_shader_handles.size());
		for (unsigned int i = 0; i < _shader_handles.size(); ++i)
		{
			shader_info& shader_info = m_shader_info_map.at(_shader_handles[i]);
			gl_shader_objects[i] = shader_info.m_gl_shader_object;
		}

		// Create shader program & link with input shader objects
		bool success = false;
		GLuint gl_shader_program_object = create_program_and_link_gl_shaders(gl_shader_objects, &success);

		// Insert shader program into map.
		shader_program_info new_program_info;
		new_program_info.m_gl_program_object = gl_shader_program_object;
		new_program_info.m_linked_shader_handles = _shader_handles;
		shader_program_handle new_shader_program_handle = m_shader_program_handle_counter;
		m_shader_program_handle_counter++;
		m_shader_program_info_map.emplace(new_shader_program_handle, new_program_info);

		// Add program handle to shader info map's "using programs map"
		for (unsigned int i = 0; i < _shader_handles.size(); ++i)
		{
			shader_info& shader_info = m_shader_info_map.at(_shader_handles[i]);
			shader_info.m_programs_using_shader.push_back(new_shader_program_handle);
		}

		return new_shader_program_handle;
	}

	GLenum ResourceManager::get_extension_shader_type(filepath_string const & _extension)
	{
		if		(_extension == ".vert")		return GL_VERTEX_SHADER;
		else if (_extension == ".frag")		return GL_FRAGMENT_SHADER;
		else								return GL_INVALID_ENUM;
	}

	bool ResourceManager::load_shader_data(
		std::filesystem::path const& _path, 
		shader_data** _out_data_ptr
	)
	{
		if (!(std::filesystem::exists(_path) && std::filesystem::is_regular_file(_path)))
		{
			Engine::Utils::print_error(
				"Could not load shader \"%s\": File does not exist.",
				_path.string().c_str()
			);
			*_out_data_ptr = nullptr;
			return false;
		}

		std::string buffer;
		std::ifstream temp_stream(_path);
		buffer.assign(std::istreambuf_iterator<char>(temp_stream), std::istreambuf_iterator<char>());
		size_t const file_size = buffer.size();

		if (file_size == 0)
		{
			Engine::Utils::print_error(
				"Could not load shader \"%s\": Shader filesize is 0.",
				_path.string().c_str()
			);
			*_out_data_ptr = nullptr;
			return false;
		}

		shader_data * shader_memory = new shader_data[file_size+1];
		if (!shader_memory)
		{
			Engine::Utils::print_error("Could not allocate memory for shader.");
			*_out_data_ptr = nullptr;
			return false;
		}
		memcpy(shader_memory, buffer.c_str(), file_size);
		shader_memory[file_size] = 0;
		*_out_data_ptr = shader_memory;

		return true;
	}

	void ResourceManager::unload_shader_data(shader_data* _shader_data)
	{
		if (_shader_data) delete[] _shader_data;
	}

	bool ResourceManager::compile_gl_shader(GLuint _gl_shader_object, shader_data* _data, GLenum _type)
	{
		Engine::Utils::assert_print_error(
			_data && _type != GL_INVALID_ENUM,
			"Invalid shader data was passed for shader creation."
		);

		GfxCall(glShaderSource(_gl_shader_object, 1, &_data, NULL));
		GfxCall(glCompileShader(_gl_shader_object));

		// Check if compilation was successful.
		GLint compiled = 0;
		glGetShaderiv(_gl_shader_object, GL_COMPILE_STATUS, &compiled);
		if (!compiled)
		{
			GLint blen = 0;
			GLsizei slen = 0;
			glGetShaderiv(_gl_shader_object, GL_INFO_LOG_LENGTH, &blen);
			char* compiler_log = new char[blen+1];
			compiler_log[blen] = 0;
			glGetInfoLogARB(_gl_shader_object, blen, &slen, compiler_log);
			Engine::Utils::print_warning("Shader compilation failed:\n%s", compiler_log);
			delete[] compiler_log;
		}
		else
			return true;
		return _gl_shader_object;
	}

	GLuint ResourceManager::create_program_and_link_gl_shaders(std::vector<GLuint> const& _gl_shader_objects, bool * _success)
	{
		Engine::Utils::assert_print_error(!_gl_shader_objects.empty(), "Could not create program: No shaders to link.");

		*_success = false;
		GLuint gl_program_object;
		GfxCallReturn(gl_program_object, glCreateProgram());
		
		attach_gl_program_shaders(gl_program_object, _gl_shader_objects);
		*_success = link_gl_program_shaders(gl_program_object);

		return gl_program_object;
	}

	void ResourceManager::attach_gl_program_shaders(GLuint _gl_program_object, std::vector<GLuint> const& _gl_shader_objects)
	{
		for (unsigned int i = 0; i < _gl_shader_objects.size(); i++)
		{
			GfxCall(glAttachShader(_gl_program_object, _gl_shader_objects[i]));
		}
	}

	bool ResourceManager::link_gl_program_shaders(GLuint _gl_program_object)
	{
		GfxCall(glLinkProgram(_gl_program_object));

		// Check if linking was successfull.
		GLint linked;
		glGetProgramiv(_gl_program_object, GL_LINK_STATUS, &linked);
		if (!linked)
		{
			GLint blen = 0;
			GLsizei slen = 0;
			glGetProgramiv(_gl_program_object, GL_INFO_LOG_LENGTH, &blen);
			if (blen > 1)
			{
				char* compiler_log = new char[blen];
				glGetInfoLogARB(_gl_program_object, blen, &slen, compiler_log);
				Engine::Utils::print_warning("Shader Program linking failed: %s", compiler_log);
				delete[] compiler_log;
			}
			return false;
		}
		return true;
	}


}
}