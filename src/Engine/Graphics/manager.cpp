#include "manager.h"

#include <limits>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <set>
#include <sstream>
#include <algorithm>

#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL.h>
#include <Engine/Utils/logging.h>

#include <STB/stb_image.h>
#include <tiny_glft/tiny_gltf.h>

#include <ImGui/imgui.h>
#include <ImGui/misc/cpp/imgui_stdlib.h>

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


	/*bool ResourceManager::LoadModel(tinygltf::TinyGLTF & _tinygltf,const char* _filepath)
	{
		std::filesystem::path const path(_filepath);
		Engine::Utils::print_base("Resource Manager", "Loading model \"%s\"...", path.string().c_str());
		std::error_code err;
		bool success = std::filesystem::exists(path, err);
		if (success)
		{
			success = false;
			std::filesystem::path const extension = path.extension();
			if (extension == ".gltf")
				success = ImportModel_GLTF(_filepath);
			else
			{
				success = false;
				Engine::Utils::print_base("Resource Manager", "Extension \"%s\" is not supported.\n", extension.u8string().c_str());
			}

			if (!success)
				Engine::Utils::print_base("Resource Manager", "ERROR: Error occurred while loading model.");
			else
				Engine::Utils::print_base("Resource Manager", "Model \"%s\" loaded successfully.", path.string().c_str());
		}
		else
			Engine::Utils::print_base("Resource Manager", "ERROR: Model at path \"%s\" does not exist.", path.string().c_str());

		return success;
	}*/

	ResourceManager::gltf_model_data const & ResourceManager::ImportModel_GLTF(const char* _filepath)
	{
		using namespace tinygltf;

		/*
				Load gltf model into internal manager.
		*/

		using namespace tinygltf;

		Model tinygltf_model;

		TinyGLTF loader;
		std::string error, warning;

		bool success = loader.LoadASCIIFromFile(&tinygltf_model, &error, &warning, _filepath);

		if (!warning.empty()) {
			Engine::Utils::print_base("TinyGLTF", "Warning: %s", warning.c_str());
			return gltf_model_data{};
		}

		if (!error.empty()) {
			Engine::Utils::print_base("TinyGLTF", "Error: %s", error.c_str());
			return gltf_model_data{};
		}

		if (!success) {
			Engine::Utils::print_base("TinyGLTF", "Failed to parse glTF.");
			return gltf_model_data{};
		}

		//decltype(m_node_data_map) new_node_data_map;
		//decltype(m_node_mesh_map) new_node_mesh_map;
		decltype(m_buffer_info_map) new_buffer_info_map;
		decltype(m_index_buffer_info_map) new_index_buffer_info_map;
		decltype(m_mesh_primitives_map) new_mesh_primitives_map;
		decltype(m_named_mesh_map) new_named_mesh_map;
		decltype(m_mesh_name_map) new_mesh_name_map;

		std::vector<mesh_handle> created_meshes;

		std::string const model_name = std::filesystem::path(_filepath).stem().string();

		/*
		* Load buffers (VBOs & IBOs)
		*/

		// Generate all GL buffers at once.
		unsigned int const new_bufferview_count = (unsigned int)tinygltf_model.bufferViews.size();
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
			tinygltf::BufferView const& read_bufferview = tinygltf_model.bufferViews[i];
			tinygltf::Buffer const& read_buffer = tinygltf_model.buffers[read_bufferview.buffer];


			buffer_info new_buffer_info;
			new_buffer_info.m_gl_id = new_gl_buffer_arr[i];
			new_buffer_info.m_target = read_bufferview.target;
			if (read_bufferview.target == 0)
			{
				Engine::Utils::print_warning("glTF Buffer View (%u) Target is undefined. Setting target as ELEMENT_ARRAY_BUFFER.", i);
				new_buffer_info.m_target = GL_ELEMENT_ARRAY_BUFFER;
			}

			// Create buffer memory block & upload data
			glBindBuffer(new_buffer_info.m_target, new_buffer_info.m_gl_id);
			// glTF buffers do not interleave binary blobs, so we can upload directly.
			GfxCall(glBufferData(
				new_buffer_info.m_target, read_bufferview.byteLength, 
				static_cast<GLvoid const*>(&read_buffer.data[read_bufferview.byteOffset]), 
				GL_STATIC_DRAW
			));

			new_buffer_info_map.emplace(curr_new_handle, new_buffer_info);
			gpu_buffer_count++;
		}

		/*
		* Set up meshes (lists of VAOs)
		*/

		unsigned int const new_mesh_count = (unsigned int)tinygltf_model.meshes.size();
		
		for (unsigned int i = 0; i < new_mesh_count; ++i)
		{
			tinygltf::Mesh const& read_mesh = tinygltf_model.meshes[i];

			mesh_handle const new_mesh_handle = m_mesh_handle_counter + i;

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
				new_primitive.m_index_buffer_handle = read_primitive.indices >= 0 
					? m_buffer_handle_counter + tinygltf_model.accessors[read_primitive.indices].bufferView
					: 0;
				new_primitive.m_render_mode = read_primitive.mode;

				glBindVertexArray(new_primitive.m_vao_gl_id);

				// Bind respective IBO (assume IBO exists for each primitive)
				if (new_primitive.m_index_buffer_handle != 0)
				{
					buffer_info const primitive_index_buffer = new_buffer_info_map.at(new_primitive.m_index_buffer_handle);
					glBindBuffer(primitive_index_buffer.m_target, primitive_index_buffer.m_gl_id);
					// Insert component type of IBO into index buffer component map if it hasn't been done already
					// Overwriting pre-existing index buffer info element is fine since it should be a copy anyways.
					auto const& indices_accessor = tinygltf_model.accessors[read_primitive.indices];
					index_buffer_info new_index_buffer_info;
					new_index_buffer_info.m_type = indices_accessor.componentType;
					new_index_buffer_info.m_index_count = indices_accessor.count;
					new_index_buffer_info_map.emplace(new_primitive.m_index_buffer_handle, new_index_buffer_info);

					// Index offset in case we are re-using same IBO for multiple primitives.
					new_primitive.m_index_byte_offset = indices_accessor.byteOffset;
					new_primitive.m_index_count = indices_accessor.count;
				}
				else
					new_primitive.m_index_count = 0;

				// Bind respective VBOs referred to by primitive attributes + index buffer

				for (auto const & primitive_attrib : read_primitive.attributes)
				{
					// Get buffer pointed to by bufferView pointed to by current attribute in primitive
					tinygltf::Accessor const& read_accessor = tinygltf_model.accessors[primitive_attrib.second];
					tinygltf::BufferView const& read_bufferview = tinygltf_model.bufferViews[read_accessor.bufferView];
					buffer_handle const attribute_buffer_handle =
						m_buffer_handle_counter + read_accessor.bufferView;
					buffer_info const attribute_referenced_buffer = new_buffer_info_map.at(attribute_buffer_handle);

					// Determine location of attribute in shader.
					// Assume position, normal and tangent attributes always exist in primitives.

					//TODO: Support joints, weights (and colors?)
					unsigned int gl_attribute_index = 0;
					if (primitive_attrib.first == "POSITION") {
						gl_attribute_index = 0;
						// Only set primitive vertex count if we are not using an IBO.
						if(new_primitive.m_index_buffer_handle == 0)
							new_primitive.m_vertex_count = read_accessor.count;
					}
					else if (primitive_attrib.first == "NORMAL") gl_attribute_index = 1;
					else if (primitive_attrib.first == "TANGENT") gl_attribute_index = 2;
					//TODO: Check if TEXCOORD actually exists rather than assuming it.
					else
					{
						unsigned int texcoord_index = 0;
						sscanf(primitive_attrib.first.c_str(), "TEXCOORD_%u", &texcoord_index);
						gl_attribute_index = 3 + texcoord_index;
					}

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

					gl_attribute_index++;
				}
				GfxCall(glBindVertexArray(0));
			}
			new_mesh_primitives_map.emplace(new_mesh_handle, std::move(curr_mesh_primitives));
			// Insert mesh into named mesh map.
			std::filesystem::path const path(_filepath);
			std::string const mesh_name = model_name + std::string("/") + read_mesh.name;
			new_named_mesh_map.emplace(mesh_name, new_mesh_handle);
			new_mesh_name_map.emplace(new_mesh_handle, mesh_name);

			created_meshes.push_back(new_mesh_handle);
		}

		/*
		* Textures / Images / Samplers
		*/

		// Add glTF source information into internal map.
		decltype(m_texture_info_map) new_texture_info_map;

		// Generate openGL textures beforehand
		std::vector<GLuint> new_gl_texture_objects;
		new_gl_texture_objects.resize(tinygltf_model.images.size());

		if(!new_gl_texture_objects.empty())
			glGenTextures((GLsizei)new_gl_texture_objects.size(), &new_gl_texture_objects[0]);
		for (unsigned int i = 0; i < tinygltf_model.images.size(); ++i)
		{
			texture_handle const current_texture = m_texture_handle_counter + i;
			tinygltf::Image & read_texture_source = tinygltf_model.images[i];
			texture_info new_texture_info;
			new_texture_info.m_gl_source_id = new_gl_texture_objects[i];
			new_texture_info.m_target = GL_TEXTURE_2D; // Assume all glTF textures are 2D.

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
					tinygltf::BufferView const& read_bufferview = tinygltf_model.bufferViews[read_texture_source.bufferView];
					stbi_uc* image_ptr = (stbi_uc*)&tinygltf_model.buffers[read_bufferview.buffer].data[read_bufferview.byteOffset];
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
			GfxCall(glBindTexture(GL_TEXTURE_2D, new_texture_info.m_gl_source_id));
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

			new_texture_info_map.emplace(current_texture, new_texture_info);
		}

		// Go through textures one more time and set parameters properly.
		for (unsigned int i = 0; i < tinygltf_model.textures.size(); ++i)
		{
			tinygltf::Texture const& read_texture = tinygltf_model.textures[i];

			texture_handle const current_texture = m_texture_handle_counter + read_texture.source;
			texture_info current_texture_info = new_texture_info_map.at(current_texture);
			glBindTexture(GL_TEXTURE_2D, current_texture_info.m_gl_source_id);
			// If a sampler is defined, use sampler to define texture parameters
			if (read_texture.sampler >= 0)
			{
				tinygltf::Sampler const& read_sampler = tinygltf_model.samplers[read_texture.sampler];

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, read_sampler.wrapS);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, read_sampler.wrapT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, read_sampler.minFilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, read_sampler.magFilter);
			}
			// If a sampler is NOT defined, sampler w/ repeat wrapping in all directions and auto filtering must be used
			// (Accoriding to GLTF specification)
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}
			glBindTexture(GL_TEXTURE_2D, 0);

		}

		/*
		*	Materials
		*/

		//TODO: Load material data into new map
		decltype(m_material_data_map) new_material_data_map;
		for (unsigned int i = 0; i < tinygltf_model.materials.size(); ++i)
		{
			tinygltf::Material const& read_material = tinygltf_model.materials[i];
			tinygltf::PbrMetallicRoughness const read_pbr_mr = read_material.pbrMetallicRoughness;

			material_handle new_material_handle = m_material_handle_counter + i;
			material_data new_material_data;

			// Set up PBR metallic roughness data
			pbr_metallic_roughness_data new_pbr_mrd;
			for (unsigned int i = 0; i < 4; i++)
			{
				new_pbr_mrd.m_base_color_factor[i] = read_material.pbrMetallicRoughness.baseColorFactor[i];
			}

			auto get_source_of_texture_index = [&](unsigned int _index)->unsigned int
			{
				return tinygltf_model.textures[_index].source;
			};

			auto determine_texture_handle = [&](int _texture_index)->texture_handle
			{
				return _texture_index == -1 ? 0 : m_texture_handle_counter + get_source_of_texture_index(_texture_index);
			};

			new_pbr_mrd.m_metallic_factor = read_pbr_mr.metallicFactor;
			new_pbr_mrd.m_roughness_factor = read_pbr_mr.roughnessFactor;
			new_pbr_mrd.m_texture_base_color = determine_texture_handle(read_pbr_mr.baseColorTexture.index);
			new_pbr_mrd.m_texture_metallic_roughness = determine_texture_handle(read_pbr_mr.metallicRoughnessTexture.index);

			// Setup material data
			new_material_data.m_pbr_metallic_roughness = new_pbr_mrd;
			new_material_data.m_texture_normal = determine_texture_handle(read_material.normalTexture.index);
			new_material_data.m_texture_occlusion = determine_texture_handle(read_material.occlusionTexture.index);
			new_material_data.m_texture_emissive = determine_texture_handle(read_material.emissiveTexture.index);

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

		// Debug output
		std::ostringstream output;
		output << "Loaded the following meshes:";
		auto iter = new_named_mesh_map.begin();
		while (iter != new_named_mesh_map.end())
		{
			output << "\n    \"" << iter->first << "\"";
			++iter;
		}
		Engine::Utils::print_base("ResourceManager", "%s", output.str().c_str());


		// Update all handle counters at the end
		// This is done so we can safely offset tinygltf resource indices based on original handle counters.
		m_mesh_handle_counter += (unsigned int)new_mesh_primitives_map.size();
		m_buffer_handle_counter += (unsigned int)new_buffer_info_map.size();
		m_material_handle_counter += (unsigned int)new_material_data_map.size();
		m_texture_handle_counter += (unsigned int)new_texture_info_map.size();

		m_buffer_info_map.merge(std::move(new_buffer_info_map));
		m_index_buffer_info_map.merge(std::move(new_index_buffer_info_map));
		m_named_mesh_map.merge(new_named_mesh_map);
		m_mesh_name_map.merge(new_mesh_name_map);
		m_mesh_primitives_map.merge(new_mesh_primitives_map);
		m_material_data_map.merge(new_material_data_map);
		m_texture_info_map.merge(new_texture_info_map);

		gltf_model_data model_data;
		model_data.m_model_name = model_name;
		model_data.m_meshes = std::move(created_meshes);

		m_imported_gltf_models.emplace(_filepath, std::move(model_data));

		return m_imported_gltf_models.at(_filepath);
	}

	ResourceManager::gltf_model_data const& ResourceManager::GetImportedGLTFModelData(const char* _filepath) const
	{
		return m_imported_gltf_models.at(_filepath);
	}

	bool ResourceManager::IsGLTFModelImported(const char* _filepath) const
	{
		return m_imported_gltf_models.find(_filepath) != m_imported_gltf_models.end();
	}

	//////////////////////////////////////////////////////////////////
	//						Buffer Methods
	//////////////////////////////////////////////////////////////////

	ResourceManager::buffer_info ResourceManager::GetBufferInfo(buffer_handle _buffer) const
	{
		return m_buffer_info_map.at(_buffer);
	}

	ResourceManager::index_buffer_info ResourceManager::GetIndexBufferInfo(buffer_handle _ibo) const
	{
		return m_index_buffer_info_map.at(_ibo);
	}

	//////////////////////////////////////////////////////////////////
	//						Mesh Methods
	//////////////////////////////////////////////////////////////////

	/*
	* Attempts to find a mesh by a given name.
	* @param	const char *	Name of mesh 
								(Mesh "Mesh_0" in file "Sponza.gltf" will be called "Sponza/Mesh_0")
	* @return	mesh_handle		Handle to mesh (0 if not found)
	*/
	ResourceManager::mesh_handle ResourceManager::FindMesh(const char* _mesh_name) const
	{
		auto iter = m_named_mesh_map.find(_mesh_name);
		return (iter == m_named_mesh_map.end()) ? 0 : iter->second;
	}

	/*
	* Gets name of given (valid) mesh
	* @param	mesh_handle		Handle to mesh
	* @returns	std::string		Name of mesh
	*/
	std::string ResourceManager::GetMeshName(mesh_handle _mesh) const
	{
		return m_mesh_name_map.at(_mesh);
	}

	/*
	* Get list of primitives belonging to given mesh
	* @param	mesh_handle				Handle to mesh
	* @return	mesh_primitive_list		List of primitives' data
	*/
	ResourceManager::mesh_primitive_list const& ResourceManager::GetMeshPrimitives(mesh_handle _mesh) const
	{
		return m_mesh_primitives_map.at(_mesh);
	}

	//////////////////////////////////////////////////////////////////
	//					Material Methods
	//////////////////////////////////////////////////////////////////

	/*
	* Get material corresponding to handle.
	* @param	material_handle		Handle to material
	* @return	material_data		Data of material
	*/
	ResourceManager::material_data ResourceManager::GetMaterial(material_handle _material) const
	{
		return m_material_data_map.at(_material);
	}

	//////////////////////////////////////////////////////////////////
	//					Texture Methods
	//////////////////////////////////////////////////////////////////

	/*
	* Creates a new texture object in graphics manager.
	* @return	texture_handle
	*/
	ResourceManager::texture_handle ResourceManager::CreateTexture(const char* _debug_name)
	{
		GLuint gl_texture_object = 0;
		glGenTextures(1, &gl_texture_object);
		texture_info new_texture_info;
		texture_handle const new_texture_handle = m_texture_handle_counter++;
		new_texture_info.m_gl_source_id = gl_texture_object;
		new_texture_info.m_target = GL_INVALID_ENUM;
		new_texture_info.m_size = glm::vec3(0, 0, 0);
		m_texture_info_map.emplace(new_texture_handle, new_texture_info);
		glObjectLabel(GL_TEXTURE, new_texture_info.m_gl_source_id, (GLsizei)strlen(_debug_name), _debug_name);
		return new_texture_handle;
	}

	/*
	* Deletes texture object corresponding to given texture handle.
	*/
	void ResourceManager::DeleteTexture(texture_handle _texture_handle)
	{
		auto iter = m_texture_info_map.find(_texture_handle);
		if (iter != m_texture_info_map.end())
		{
			glDeleteTextures(1, &iter->second.m_gl_source_id);
			m_texture_info_map.erase(iter);
		}
	}

	/*
	* Bind texture object corresponding to given texture handle.
	*/
	void ResourceManager::BindTexture(texture_handle _texture_handle) const
	{
		auto iter = m_texture_info_map.find(_texture_handle);
		Engine::Utils::assert_print_error(iter != m_texture_info_map.end(), "Texture handle is invalid.");
		Engine::Utils::assert_print_error(iter->second.m_target != GL_INVALID_ENUM, "Texture has no target assigned.");
		glBindTexture(iter->second.m_target, iter->second.m_gl_source_id);
	}

	/*
	* Get texture information corresponding to given texture handle.
	* @param	texture_handle		Handle to texture
	* @return	texture_info		Information about texture
	*/
	ResourceManager::texture_info ResourceManager::GetTextureInfo(texture_handle _texture_handle) const
	{
		return m_texture_info_map.at(_texture_handle);
	}

	/*
	* Specify layout of texture object corresponding to given texture handle
	* @param	texture_handle
	* @param	GLint			Format that object will use internally (i.e. GL_R, GL_RGBA, etc...)
	* @param	glm::uvec2		Size of object in terms of texels.
	* @param	uint			Mipmap level of texture
	*/
	void ResourceManager::AllocateTextureStorage2D(
		texture_handle _texture_handle, GLenum _internal_format, 
		glm::uvec2 _size, 
		texture_parameters _params,
		unsigned int _texture_levels
	)
	{
		assert(_texture_levels >= 1);
		texture_info& tex_info = set_texture_target_and_bind(_texture_handle, GL_TEXTURE_2D);
		GfxCall(glTexStorage2D(
			tex_info.m_target, _texture_levels, _internal_format,
			(GLsizei)_size.x, (GLsizei)_size.y
		));
		tex_info.m_size = glm::uvec3(_size, 1);
		SetTextureParameters(_texture_handle, _params);
	}

	/*
	* Specify layout AND upload data to texture object corresponding to given texture handle
	* @param	texture_handle
	* @param	GLint			Format that object will use internally (i.e. GL_R, GL_RGBA, etc...)
	* @param	glm::uvec2		Size of object in terms of texels.
	* @param	uint			Mipmap level of texture
	* @param	GLenum			Input format that data will be given in (i.e. GL_R, GL_RGBA, etc...)
	* @param	GLenum			Input format component type (i.e. GL_UNSIGNED_BYTE, etc...)
	* @param	void *			Pointer to texture data that will be uploaded. Can be nullptr.
	*/
	void ResourceManager::SpecifyAndUploadTexture2D(
		texture_handle _texture_handle, GLint _internal_format, glm::uvec2 _size, unsigned int _texture_levels,
		GLenum _input_format, GLenum _input_component_type, void* _data
	)
	{
		texture_info & tex_info = set_texture_target_and_bind(_texture_handle, GL_TEXTURE_2D);
		GfxCall(glTexImage2D(
			tex_info.m_target, _texture_levels,
			_internal_format, (GLsizei)_size.x, (GLsizei)_size.y, 0, 
			_input_format, _input_component_type, _data
		));
		GfxCall(glGenerateMipmap(tex_info.m_target));
		tex_info.m_size = glm::uvec3(_size, 1);
	}

	/*
	* Sets texture parameters of given texture.
	* @param	texture_handle		Given texture to set parameters of.
	* @param	texture_parameters	Parameters to set.
	*/
	void ResourceManager::SetTextureParameters(texture_handle _texture_handle, texture_parameters _params)
	{
		auto iter = m_texture_info_map.find(_texture_handle);
		Engine::Utils::assert_print_error(iter != m_texture_info_map.end(), "Invalid texture handle.");
		texture_info const tex_info = iter->second;
		GfxCall(glBindTexture(tex_info.m_target, tex_info.m_gl_source_id));
		switch (tex_info.m_target)
		{
		case GL_TEXTURE_3D:	
			GfxCall(glTexParameteri(tex_info.m_target, GL_TEXTURE_WRAP_R, _params.m_wrap_r)); 
			[[fallthrough]];
		case GL_TEXTURE_2D:	
			GfxCall(glTexParameteri(tex_info.m_target, GL_TEXTURE_WRAP_T, _params.m_wrap_t)); 
			[[fallthrough]];
		case GL_TEXTURE_1D: 
			GfxCall(glTexParameteri(tex_info.m_target, GL_TEXTURE_WRAP_S, _params.m_wrap_s)); 
		}
		GfxCall(glTexParameteri(tex_info.m_target, GL_TEXTURE_MAG_FILTER, _params.m_mag_filter));
		GfxCall(glTexParameteri(tex_info.m_target, GL_TEXTURE_MIN_FILTER, _params.m_min_filter));
	}

	/*
	* Set target of texture.
	* @param	texture_handle		Given texture to set target of.
	* @param	GLenum				Target to assign to texture object.
	*/
	ResourceManager::texture_info & ResourceManager::set_texture_target_and_bind(texture_handle _texture_handle, GLenum _target)
	{
		auto iter = m_texture_info_map.find(_texture_handle);
		Engine::Utils::assert_print_error(iter != m_texture_info_map.end(), "Invalid texture handle.");
		texture_info& input_texture_info = iter->second;
		input_texture_info.m_target = _target;
		GfxCall(glBindTexture(input_texture_info.m_target, input_texture_info.m_gl_source_id));
		return iter->second;
	}

	//////////////////////////////////////////////////////////////////
	//						Framebuffer Methods
	//////////////////////////////////////////////////////////////////

	ResourceManager::framebuffer_handle ResourceManager::CreateFramebuffer()
	{
		GLuint framebuffer_object = 0;
		glGenFramebuffers(1, &framebuffer_object);
		framebuffer_handle const new_framebuffer_handle = m_framebuffer_handle_counter++;
		framebuffer_info new_framebuffer_info;
		new_framebuffer_info.m_gl_object = framebuffer_object;
		new_framebuffer_info.m_target = GL_FRAMEBUFFER;
		m_framebuffer_info_map.emplace(new_framebuffer_handle, new_framebuffer_info);
		return new_framebuffer_handle;
	}

	void ResourceManager::DeleteFramebuffer(framebuffer_handle _framebuffer)
	{
		auto iter = m_framebuffer_info_map.find(_framebuffer);
		Engine::Utils::assert_print_error(iter != m_framebuffer_info_map.end(), "Invalid framebuffer handle.");
		framebuffer_info const framebuffer_info = iter->second;
		m_framebuffer_info_map.erase(iter);
		glDeleteFramebuffers(1, & framebuffer_info.m_gl_object);
	}

	ResourceManager::framebuffer_info ResourceManager::BindFramebuffer(framebuffer_handle _framebuffer) const
	{
		auto iter = m_framebuffer_info_map.find(_framebuffer);
		Engine::Utils::assert_print_error(iter != m_framebuffer_info_map.end(), "Invalid framebuffer handle.");
		framebuffer_info const framebuffer_info = iter->second;
		glBindFramebuffer(framebuffer_info.m_target, framebuffer_info.m_gl_object);
		return framebuffer_info;
	}

	void ResourceManager::UnbindFramebuffer(GLenum _framebuffer_target) const
	{
		glBindFramebuffer(_framebuffer_target, 0);
	}


	void ResourceManager::AttachTextureToFramebuffer(framebuffer_handle _framebuffer, GLenum _attachment_point, texture_handle _texture)
	{
		framebuffer_info const framebuffer_info = BindFramebuffer(_framebuffer);
		texture_info const texture_info = m_texture_info_map.at(_texture);
		assert(texture_info.m_target == GL_TEXTURE_2D);
		GfxCall(glFramebufferTexture2D(framebuffer_info.m_target, _attachment_point, texture_info.m_target, texture_info.m_gl_source_id, 0));
		GLuint const complete_status = glCheckFramebufferStatus(framebuffer_info.m_target);
		Engine::Utils::assert_print_error(
			complete_status == GL_FRAMEBUFFER_COMPLETE,
			"Framebuffer is not complete. Status 0x%X.", complete_status
		);
	}

	void ResourceManager::DrawFramebuffers(framebuffer_handle _framebuffer, unsigned int _arr_size, GLenum const* _arr_attachment_points) const
	{
		framebuffer_info const info = m_framebuffer_info_map.at(_framebuffer);
		GfxCall(glNamedFramebufferDrawBuffers(info.m_gl_object, (GLsizei)_arr_size, _arr_attachment_points));
		GLuint const complete_status = glCheckNamedFramebufferStatus(info.m_gl_object, info.m_target);
		Engine::Utils::assert_print_error(
			complete_status == GL_FRAMEBUFFER_COMPLETE,
			"Framebuffer is not complete. Status 0x%X.", complete_status
		);
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
			shader_handle curr_output_shader_handle = 0;

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

			// Attach debug name to shader object
			glObjectLabel(GL_SHADER, new_shader_info.m_gl_shader_object, (GLsizei)path_str.size(), path_str.c_str());

			unload_shader_data(loaded_shader_data);
		}

		m_shader_handle_counter += new_shader_count;

		m_filepath_shader_map.merge(new_filepath_shader_map);
		m_shader_info_map.merge(new_shader_info_map);

		return output_shader_handles;
	}

	ResourceManager::shader_program_handle ResourceManager::LoadShaderProgram(std::string _program_name, std::vector<std::filesystem::path> _shader_filepaths)
	{
		// Get shader handles corresponding to given shader filepaths from internal map
		std::vector<shader_handle> shader_handles;
		shader_handles.resize(_shader_filepaths.size());
		for (unsigned int i = 0; i < _shader_filepaths.size(); ++i)
			shader_handles[i] = m_filepath_shader_map.at(_shader_filepaths[i].string());

		return LoadShaderProgram(_program_name, shader_handles);
	}

	ResourceManager::shader_program_handle ResourceManager::FindShaderProgram(std::string _program_name) const
	{
		auto iter = m_named_shader_program_map.find(_program_name);
		return iter == m_named_shader_program_map.end() ? 0 : iter->second;
	}

	ResourceManager::shader_program_handle ResourceManager::LoadShaderProgram(std::string _program_name, std::vector<shader_handle> _shader_handles)
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
		m_named_shader_program_map.emplace(_program_name, new_shader_program_handle);

		// Add program handle to shader info map's "using programs map"
		for (unsigned int i = 0; i < _shader_handles.size(); ++i)
		{
			shader_info& shader_info = m_shader_info_map.at(_shader_handles[i]);
			shader_info.m_programs_using_shader.push_back(new_shader_program_handle);
		}

		// Attach debug name to shader program
		glObjectLabel(GL_PROGRAM, new_program_info.m_gl_program_object, (GLsizei)_program_name.size(), _program_name.c_str());

		return new_shader_program_handle;
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

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::uvec2 _uniform_value)
	{
		GfxCall(glProgramUniform2ui(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec2 _uniform_value)
	{
		GfxCall(glProgramUniform2f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec3 _uniform_value)
	{
		GfxCall(glProgramUniform3f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y, _uniform_value.z));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::vec4 _uniform_value)
	{
		GfxCall(glProgramUniform4f(m_bound_gl_program_object, _uniform_location, _uniform_value.x, _uniform_value.y, _uniform_value.z, _uniform_value.w));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::mat3 const & _uniform_value)
	{
		GfxCall(glProgramUniformMatrix3fv(m_bound_gl_program_object, _uniform_location, 1, false, glm::value_ptr(_uniform_value)));
	}

	void ResourceManager::SetBoundProgramUniform(unsigned int _uniform_location, glm::mat4 const & _uniform_value)
	{
		GfxCall(glProgramUniformMatrix4fv(m_bound_gl_program_object, _uniform_location, 1, false, glm::value_ptr(_uniform_value)));
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

	//////////////////////////////////////////////////////////////////
	//					Graphics Resource Management
	//////////////////////////////////////////////////////////////////

	void ResourceManager::Reset()
	{
		DeleteAllGraphicsResources();
		reset_counters();
	}

	void ResourceManager::DeleteAllGraphicsResources()
	{
		auto collect_keys = [](auto const& map, auto& output) {
			std::transform(
				map.begin(), map.end(), std::back_inserter(output),
				[](auto iter)->mesh_handle {return iter.first; }
			);
		};

		// Collect all mesh handles
		std::vector<mesh_handle> mesh_handles;
		std::vector<buffer_handle> buffer_handles;
		std::vector<texture_handle> texture_handles;
		std::vector<framebuffer_handle> framebuffer_handles;
		std::vector<shader_handle> shader_handles;
		std::vector<shader_program_handle> shader_program_handles;

		collect_keys(m_mesh_primitives_map, mesh_handles);
		collect_keys(m_buffer_info_map, buffer_handles);
		collect_keys(m_texture_info_map, texture_handles);
		collect_keys(m_framebuffer_info_map, framebuffer_handles);
		collect_keys(m_shader_info_map, shader_handles);
		collect_keys(m_shader_program_info_map, shader_program_handles);

		delete_meshes(mesh_handles);
		delete_buffers(buffer_handles);
		delete_textures(texture_handles);
		delete_framebuffers(framebuffer_handles);
		delete_shaders(shader_handles);
		delete_programs(shader_program_handles);

		m_imported_gltf_models.clear();
		m_material_data_map.clear();
	}

	void ResourceManager::reset_counters()
	{
		m_mesh_handle_counter = 1;
		m_buffer_handle_counter = 1;
		m_material_handle_counter = 1;
		m_texture_handle_counter = 1;
		m_framebuffer_handle_counter = 1;
		m_shader_handle_counter = 1;
		m_shader_program_handle_counter = 1;
	}

	void ResourceManager::delete_meshes(std::vector<mesh_handle> const& _meshes)
	{
		// Collect all gl vertex array objects
		std::vector<GLuint> gl_vertex_array_objects;

		// Iterate over all meshes
		for (unsigned int i = 0; i < _meshes.size(); ++i)
		{
			auto mesh_iter = m_mesh_primitives_map.find(_meshes[i]);
			// Each mesh has a list of primitives, each of which contains a VAO
			for (mesh_primitive_data primitive_data : mesh_iter->second)
			{
				gl_vertex_array_objects.push_back(primitive_data.m_vao_gl_id);
			}
			m_mesh_primitives_map.erase(mesh_iter);

		}

		// Delete mesh entries in map.
		for (unsigned int i = 0; i < _meshes.size(); ++i)
		{
			mesh_handle const delete_mesh = _meshes[i];
			auto iter = m_mesh_name_map.find(delete_mesh);
			if (iter != m_mesh_name_map.end())
			{
				m_named_mesh_map.erase(iter->second);
				m_mesh_name_map.erase(iter);
			}
		}

		// Delete all gl texture objects simultaneously
		if (!gl_vertex_array_objects.empty())
		{
			GfxCall(glDeleteVertexArrays((GLsizei)gl_vertex_array_objects.size(), &gl_vertex_array_objects[0]));
		}
	}

	/*
	* Delete instances of buffer info and their corresponding gl objects
	* @param	std::vector<buffer_handle> const &		List of buffers to delete
	*/
	void ResourceManager::delete_buffers(std::vector<buffer_handle> const& _buffers)
	{
		// Collect all gl buffer objects
		std::vector<GLuint> gl_buffer_objects;
		for (unsigned int i = 0; i < _buffers.size(); ++i)
		{
			m_index_buffer_info_map.erase(_buffers[i]);
			auto buffer_info_iter = m_buffer_info_map.find(_buffers[i]);
			gl_buffer_objects.push_back(buffer_info_iter->second.m_gl_id);
			m_buffer_info_map.erase(_buffers[i]);
		}
		// Delete all gl buffer objects simultaneously
		if (!gl_buffer_objects.empty())
		{
			GfxCall(glDeleteBuffers((GLsizei)gl_buffer_objects.size(), &gl_buffer_objects[0]));
		}
	}

	/*
	* Delete instances of material information.
	* @param	std::vector<material_handle> const &	List of materials to delete
	* @detail	Materials do not have GL objects to delete.
	*/
	void ResourceManager::delete_materials(std::vector<material_handle> const& _materials)
	{
		for (unsigned int i = 0; i < _materials.size(); ++i)
			m_material_data_map.erase(_materials[i]);
	}

	/*
	* Delete instances of texture information and thei corresponding GL objects
	* @param	std::vector<texture_handle> const &		List of textures to delete
	*/
	void ResourceManager::delete_textures(std::vector<texture_handle> const& _textures)
	{
		// Collect all gl texture objects
		std::vector<GLuint> gl_texture_objects;
		for (unsigned int i = 0; i < _textures.size(); ++i)
		{
			auto texture_info_iter = m_texture_info_map.find(_textures[i]);
			gl_texture_objects.push_back(texture_info_iter->second.m_gl_source_id);
			m_texture_info_map.erase(_textures[i]);
		}
		// Delete all gl texture objects simultaneously
		if (!gl_texture_objects.empty())
		{
			GfxCall(glDeleteTextures((GLsizei)gl_texture_objects.size(), &gl_texture_objects[0]));
		}
	}

	void ResourceManager::delete_framebuffers(std::vector<framebuffer_handle> const& _framebuffers)
	{
		// Collect all gl framebuffer objects
		std::vector<GLuint> gl_objects;
		for (unsigned int i = 0; i < _framebuffers.size(); ++i)
		{
			auto info_iter = m_framebuffer_info_map.find(_framebuffers[i]);
			gl_objects.push_back(info_iter->second.m_gl_object);
			m_framebuffer_info_map.erase(_framebuffers[i]);
		}
		// Delete all gl framebuffer objects simultaneously
		if (!gl_objects.empty())
		{
			GfxCall(glDeleteFramebuffers((GLsizei)gl_objects.size(), &gl_objects[0]));
		}
	}

	void ResourceManager::delete_shaders(std::vector<shader_handle> _shaders)
	{
		// Remove entry in name to shader map
		std::sort(_shaders.begin(), _shaders.end());
		auto iter = m_filepath_shader_map.begin();
		while (iter != m_filepath_shader_map.end())
		{
			if (std::binary_search(_shaders.begin(), _shaders.end(), iter->second))
				iter = m_filepath_shader_map.erase(iter);
			else
				++iter;
		}
		// Remove entry in shader info map
		for (unsigned int i = 0; i < _shaders.size(); ++i)
		{
			shader_handle shader = _shaders[i];
			auto iter = m_shader_info_map.find(shader);
			GfxCall(glDeleteShader(iter->second.m_gl_shader_object));
			m_shader_info_map.erase(iter);
		}
	}

	void ResourceManager::delete_programs(std::vector<shader_program_handle>  _programs)
	{
		// Remove entry in name to shader map
		std::sort(_programs.begin(), _programs.end());
		auto iter = m_named_shader_program_map.begin();
		while (iter != m_named_shader_program_map.end())
		{
			if (std::binary_search(_programs.begin(), _programs.end(), iter->second))
				iter = m_named_shader_program_map.erase(iter);
			else
				++iter;
		}
		// Remove entry in shader program info map
		for (unsigned int i = 0; i < _programs.size(); ++i)
		{
			shader_program_handle program = _programs[i];
			auto iter = m_shader_program_info_map.find(_programs[i]);

			// If a deleted program is bound, unbind it.
			if (m_bound_program == program)
			{
				glUseProgram(0);
				m_bound_program = 0;
				m_bound_gl_program_object = 0;
			}

			GfxCall(glDeleteProgram(iter->second.m_gl_program_object));
			m_shader_program_info_map.erase(iter);
		}
	}


	enum E_ResourceType {eMesh, eCOUNT};
	static E_ResourceType s_editor_show_resource_list = eMesh;

	static std::unordered_map<E_ResourceType, const char*> s_resource_type_name{
		{E_ResourceType::eMesh, "Mesh"}
	};

	const char* get_type_name(E_ResourceType _type) { return s_resource_type_name.at(_type); }

	void ResourceManager::EditorDisplay()
	{

		if (ImGui::Begin("Graphics Resources"))
		{
			if (ImGui::BeginCombo("Display Resource", get_type_name(s_editor_show_resource_list)))
			{
				for (unsigned int i = 0; i < E_ResourceType::eCOUNT; ++i)
				{
					E_ResourceType const type = E_ResourceType(i);
					if (ImGui::Selectable(get_type_name(type)))
						s_editor_show_resource_list = type;
				}
				ImGui::EndCombo();
			}

			ImGui::Separator();

			if (s_editor_show_resource_list == E_ResourceType::eMesh)
				editor_mesh_list();
		}
		ImGui::End();
	}

	void ResourceManager::editor_mesh_list()
	{
		static std::string s_search_bar{};
		static std::unordered_map<std::string, mesh_handle> partial_name_matches;

		if (ImGui::IsWindowAppearing())
		{
			s_search_bar.clear();
			partial_name_matches.clear();
		}

		if (ImGui::InputText("Search", &s_search_bar, ImGuiInputTextFlags_CharsNoBlank))
		{
			partial_name_matches.clear();
			std::string search_lowercase;
			std::transform(s_search_bar.begin(), s_search_bar.end(), std::back_inserter(search_lowercase), std::tolower);
			for (auto const & pair : m_named_mesh_map)
			{
				std::string mesh_name_lowercase;
				std::transform(pair.first.begin(), pair.first.end(), std::back_inserter(mesh_name_lowercase), std::tolower);
				size_t result = mesh_name_lowercase.find(search_lowercase);
				if (result != std::string::npos)
					partial_name_matches.emplace(pair);
			}
		}
		auto const & map = (partial_name_matches.empty() && s_search_bar.empty()) ? m_named_mesh_map : partial_name_matches;
		for (auto const& match : map)
		{
			ImGui::Selectable(match.first.c_str());
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("RESOURCE_MESH", (void const*)&match.second, sizeof(mesh_handle));
				ImGui::EndDragDropSource();
			}
		}

	}


}
}